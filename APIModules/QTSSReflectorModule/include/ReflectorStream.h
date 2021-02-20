/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
/*
    File:       ReflectorStream.h

    Contains:   This object supports reflecting an RTP multicast stream to N
                RTPStreams. It spaces out the packet send times in order to
                maximize the randomness of the sending pattern and smooth
                the stream.
*/

#ifndef _REFLECTOR_STREAM_H_
#define _REFLECTOR_STREAM_H_

#include <CF/Thread/IdleTask.h>
#include <CF/Net/Socket/UDPSocket.h>
#include <CF/Net/Socket/UDPSocketPool.h>
#include <CF/Net/Socket/UDPDemuxer.h>

#include "QTSS.h"

#include "SourceInfo.h"
#include "SequenceNumberMap.h"

#include "RTCPSRPacket.h"
#include "ReflectorOutput.h"

#include "RTPProtocol.h"

/*fantasy add this*/
#include "KeyFrameCache.h"

#if EVENT_EDGE_TRIGGERED_SUPPORTED
#define STREAM_USE_ET 1
#else
#define STREAM_USE_ET 0
#endif

//This will add some printfs that are useful for checking the thinning
#define REFLECTOR_THINNING_DEBUGGING 0
#define MAX_CACHE_SIZE  (1024*1024*2)

//Define to use new potential workaround for NAT problems
#define NAT_WORKAROUND 1

class ReflectorPacket;
class ReflectorSender;
class ReflectorStream;
class RTPSessionOutput;
class ReflectorSession;


class ReflectorPacket {
 public:

  ReflectorPacket() : fQueueElem() {
    fQueueElem.SetEnclosingObject(this);
    this->Reset();
  }

  /**
   * make packet ready to reuse
   * @note fQueueElem is always point to this
   */
  void Reset() {
    fBucketsSeenThisPacket = 0;
    fTimeArrived = 0;
    fPacketPtr.Set(fPacketData, 0);
    fIsRTCP = false;
    fStreamCountID = 0;
    fNeededByOutput = false;
  }

  ~ReflectorPacket() = default;

  void SetPacketData(char *data, UInt32 len, bool isRTCP) {
    Assert(kMaxReflectorPacketSize > len);

    if (len > kMaxReflectorPacketSize)
      len = kMaxReflectorPacketSize;

    if (len > 0)
      memcpy(this->fPacketPtr.Ptr, data, len);
    this->fPacketPtr.Len = len;
    this->fIsRTCP = isRTCP;
  }

  bool IsRTCP() { return fIsRTCP; }

  inline UInt32 GetPacketRTPTime();
  inline UInt16 GetPacketRTPSeqNum();
  inline UInt32 GetSSRC();
  inline SInt64 GetPacketNTPTime();

 private:

  enum {
    kMaxReflectorPacketSize = 2060    //jm 5/02 increased from 2048 by 12 bytes for test bytes appended to packets
  };

  UInt64 fStreamCountID;
  SInt64 fTimeArrived;
  UInt32 fBucketsSeenThisPacket;
  bool fIsRTCP; // the first field we set at beginning of ReflectorSocket::ProcessPacket
  bool fNeededByOutput; // is this packet still needed for output?

  CF::QueueElem fQueueElem;

  CF::StrPtrLen fPacketPtr;
  char fPacketData[kMaxReflectorPacketSize];

  friend class ReflectorSender;
  friend class ReflectorSocket;
  friend class RTPSessionOutput;
};

UInt32 ReflectorPacket::GetSSRC() {
  if (fPacketPtr.Ptr == nullptr) return 0;

  auto *theSsrcPtr = (UInt32 *) fPacketPtr.Ptr;

  if (fIsRTCP) {
    if (fPacketPtr.Len < 8) return 0;
    return ntohl(theSsrcPtr[1]); // RTCP
  } else {
    if (fPacketPtr.Len < 12) return 0;
    return ntohl(theSsrcPtr[2]); // RTP SSRC
  }
}

UInt32 ReflectorPacket::GetPacketRTPTime() {
  if (fPacketPtr.Ptr == nullptr) return 0;

  auto *theTimestampPtr = (UInt32 *) fPacketPtr.Ptr;

  if (!fIsRTCP) {
    //The RTP timestamp number is the second long of the packet
    if (fPacketPtr.Len < 8) return 0;
    return ntohl(theTimestampPtr[1]);
  } else {
    if (fPacketPtr.Len < 20) return 0;
    return ntohl(theTimestampPtr[4]);
  }
}

UInt16 ReflectorPacket::GetPacketRTPSeqNum() {
  Assert(!fIsRTCP); // not a supported type

  if (fPacketPtr.Ptr == nullptr || fPacketPtr.Len < 4 || fIsRTCP) return 0;

  return ntohs(((UInt16 *) fPacketPtr.Ptr)[1]); //The RTP sequenc number is the second short of the packet
}

SInt64 ReflectorPacket::GetPacketNTPTime() {
  Assert(fIsRTCP); // not a supported type

  if (fPacketPtr.Ptr == nullptr || fPacketPtr.Len < 16 || !fIsRTCP) return 0;

  auto *theReport = (UInt32 *) fPacketPtr.Ptr;

  SInt64 ntp = 0;
  ::memcpy(&ntp, &theReport[2], sizeof(SInt64));

  return CF::Core::Time::Time1900Fixed64Secs_To_TimeMilli(CF::Utils::NetworkToHostSInt64(ntp));
}

/**
 * Custom UDP socket classes for doing reflector packet retrieval, socket management
 *
 * @note  非阻塞，边沿触发，Demuxer
 */
class ReflectorSocket
    : public CF::Thread::IdleTask,
      public CF::Net::UDPSocket {
 public:

  explicit ReflectorSocket();
  ~ReflectorSocket() override;

  void AddBroadcasterSession(QTSS_ClientSessionObject inSession) {
    CF::Core::MutexLocker locker(this->GetDemuxer()->GetMutex());
    fBroadcasterClientSession = inSession;
  }

  void RemoveBroadcasterSession(QTSS_ClientSessionObject inSession) {
    CF::Core::MutexLocker locker(this->GetDemuxer()->GetMutex());
    if (inSession == fBroadcasterClientSession)
      fBroadcasterClientSession = nullptr;
  }

  void AddSender(ReflectorSender *inSender);

  void RemoveSender(ReflectorSender *inStreamElem);

  bool HasSender() {
    return (this->GetDemuxer()->GetHashTable()->GetNumEntries() > 0);
  }

  void BufferKeyFrame(ReflectorSender *theSender, ReflectorPacket *thePacket);

  void ProcessPacket(const SInt64 &inMilliseconds, ReflectorPacket *thePacket, UInt32 theRemoteAddr, UInt16 theRemotePort);

  ReflectorPacket *GetPacket();

  SInt64 Run() override;

  void SetSSRCFilter(bool state, UInt32 timeoutSecs) {
    fFilterSSRCs = state;
    fTimeoutSecs = timeoutSecs;
  }

 private:

  void GetIncomingData(const SInt64 &inMilliseconds);

  bool FilterInvalidSSRCs(ReflectorPacket *thePacket);

  //Number of packets to allocate when the socket is first created
  enum {
    kNumPreallocatedPackets = 20,   //UInt32
    kRefreshBroadcastSessionIntervalMilliSecs = 10000,
    kSSRCTimeOut = 30000 // milliseconds before clearing the SSRC if no new ssrcs have come in
  };
  QTSS_ClientSessionObject fBroadcasterClientSession;
  SInt64 fLastBroadcasterTimeOutRefresh;

  CF::Queue fFreeQueue;   // Queue of available ReflectorPackets
  CF::Queue fSenderQueue; // Queue of senders
  SInt64 fSleepTime;

  UInt32 fValidSSRC;
  SInt64 fLastValidSSRCTime; // 上一次接收到有效SSRC包的时间
  bool fFilterSSRCs;
  UInt32 fTimeoutSecs;

  bool fHasReceiveTime;
  UInt64 fFirstReceiveTime;
  SInt64 fFirstArrivalTime;
  UInt32 fCurrentSSRC;
};

class ReflectorSocketPool : public CF::Net::UDPSocketPool {
 public:
  ReflectorSocketPool() = default;
  ~ReflectorSocketPool() override = default;

  CF::Net::UDPSocketPair *ConstructUDPSocketPair() override;
  void DestructUDPSocketPair(CF::Net::UDPSocketPair *inPair) override;
  void SetUDPSocketOptions(CF::Net::UDPSocketPair *inPair) override;

  void DestructUDPSocket(ReflectorSocket *socket);
};

/**
 * ReflectorSender 与 ReflectorStream.fStreamInfo.fSrcIPAddr 绑定
 */
class ReflectorSender : public CF::Net::UDPDemuxerTask {
 public:
  ReflectorSender(ReflectorStream *inStream, UInt32 inWriteFlag);
  ~ReflectorSender() override;

  // Used for adjusting sequence numbers in light of thinning
  UInt16 GetPacketSeqNumber(const CF::StrPtrLen &inPacket);

  void SetPacketSeqNumber(const CF::StrPtrLen &inPacket, UInt16 inSeqNumber);

  bool PacketShouldBeThinned(QTSS_RTPStreamObject inStream, const CF::StrPtrLen &inPacket);

  // We want to make sure that ReflectPackets only gets invoked when there
  // is actually work to do, because it is an expensive function
  bool ShouldReflectNow(const SInt64 &inCurrentTime, SInt64 *ioWakeupTime);

  // This function gets data from the multicast source and reflects.
  // Returns the time at which it next needs to be invoked
  void ReflectPackets(SInt64 *ioWakeupTime, CF::Queue *inFreeQueue);

  // this is the old way of doing reflect packets. It is only here until the relay code can be cleaned up.
  void ReflectRelayPackets(SInt64 *ioWakeupTime, CF::Queue *inFreeQueue);

  CF::QueueElem *SendPacketsToOutput(ReflectorOutput *theOutput, CF::QueueElem *currentPacket,
                                     SInt64 currentTime, SInt64 bucketDelay, bool firstPacket);

  UInt32 GetOldestPacketRTPTime(bool *foundPtr);

  UInt16 GetFirstPacketRTPSeqNum(bool *foundPtr);

  bool GetFirstPacketInfo(UInt16 *outSeqNumPtr, UInt32 *outRTPTimePtr, SInt64 *outArrivalTimePtr);

  CF::QueueElem *GetClientBufferNextPacketTime(UInt32 inRTPTime);

  bool GetFirstRTPTimePacket(UInt16 *outSeqNumPtr, UInt32 *outRTPTimePtr, SInt64 *outArrivalTimePtr);

  void RemoveOldPackets(CF::Queue *inFreeQueue);

  CF::QueueElem *GetClientBufferStartPacketOffset(SInt64 offsetMsec, bool needKeyFrameFirstPacket = false);

  CF::QueueElem *GetClientBufferStartPacket() {
    return this->GetClientBufferStartPacketOffset(0);
  };

  // ->geyijyn@20150427
  // 关键帧索引及丢帧方案
  CF::QueueElem *NeedRelocateBookMark(CF::QueueElem *currentElem);

  CF::QueueElem *GetNewestKeyFrameFirstPacket(CF::QueueElem *currentElem, SInt64 offsetMsec);

  bool IsKeyFrameFirstPacket(ReflectorPacket *thePacket);

  ReflectorStream *fStream;
  UInt32 fWriteFlag; // 标记 RTP/RTCP

  CF::Queue fPacketQueue;
  CF::QueueElem *fFirstNewPacketInQueue; // set in ReflectorSocket::ProcessPacket, and clear in ReflectorSender::ReflectPackets
  CF::QueueElem *fFirstPacketInQueueForNewOutput;
  CF::QueueElem *fKeyFrameStartPacketElementPointer; // 最新关键帧指针

  //these serve as an optimization, keeping track of when this
  //sender needs to run so it doesn't run unnecessarily

  inline void SetNextTimeToRun(SInt64 nextTime) {
    fNextTimeToRun = nextTime;
    //s_printf("SetNextTimeToRun =%"_64BITARG_"d\n", fNextTimeToRun);
  }

  bool fHasNewPackets; // the flag of new packet arrived
  SInt64 fNextTimeToRun; // real time

  // how often to send RRs to the source
  enum {
    kRRInterval = 5000      //SInt64 (every 5 seconds)
  };
  SInt64 fLastRRTime;

  CF::QueueElem fSocketQueueElem;

  friend class ReflectorSocket;
  friend class ReflectorStream;
};

/**
 * Inbound stream
 */
class ReflectorStream {
 public:

  enum {
    // A ReflectorStream is uniquely identified by the
    // destination IP address & destination port of the broadcast.
    // This ID simply contains that information.
    //
    // A unicast broadcast can also be identified by source IP address. If
    // you are attempting to demux by source IP, this ID will not guarentee
    // uniqueness and special care should be used.
    kStreamIDSize = sizeof(UInt32) + sizeof(UInt16)
  };

  // Uses a StreamInfo to generate a unique ID
  static void GenerateSourceID(SourceInfo::StreamInfo *inInfo, char *ioBuffer);

  explicit ReflectorStream(SourceInfo::StreamInfo *inInfo);
  ~ReflectorStream();

  //
  // SETUP
  //
  // Call Register from the Register role, as this object has some QTSS API
  // attributes to setup
  static void Register();

  static void Initialize(QTSS_ModulePrefsObject inPrefs);

  //
  // MODIFIERS

  // Call this to initialize the reflector sockets. Uses the QTSS_RTSPRequestObject
  // if provided to report any errors that occur
  // Passes the QTSS_ClientSessionObject to the socket so the socket can update the session if needed.
  QTSS_Error BindSockets(QTSS_StandardRTSP_Params *inParams, UInt32 inReflectorSessionFlags, bool filterState, UInt32 timeout);

  // This stream reflects packets from the broadcast to specific ReflectorOutputs.
  // You attach outputs to ReflectorStreams this way. You can force the ReflectorStream
  // to put this output into a certain bucket by passing in a certain bucket index.
  // Pass in -1 if you don't care. AddOutput returns the bucket index this output was
  // placed into, or -1 on an error.

  SInt32 AddOutput(ReflectorOutput *inOutput, SInt32 putInThisBucket);

  // Removes the specified output from this ReflectorStream.
  void RemoveOutput(ReflectorOutput *inOutput); // Removes this output from all tracks

  void TearDownAllOutputs(); // causes a tear down and then a remove

  // If the incoming data is RTSP interleaved, packets for this stream are identified
  // by channel numbers
  void SetRTPChannelNum(SInt16 inChannel) { fRTPChannel = inChannel; }

  void SetRTCPChannelNum(SInt16 inChannel) { fRTCPChannel = inChannel; }

  void PushPacket(char *packet, UInt32 packetLen, bool isRTCP);

  //
  // ACCESSORS
  UInt32 GetBitRate() { return fCurrentBitRate; }

  SourceInfo::StreamInfo *GetStreamInfo() { return &fStreamInfo; }

  CF::Core::Mutex *GetMutex() { return &fBucketMutex; }

  void *GetStreamCookie() { return this; }

  SInt16 GetRTPChannel() { return fRTPChannel; }

  SInt16 GetRTCPChannel() { return fRTCPChannel; }

  CF::Net::UDPSocketPair *GetSocketPair() { return fSockets; }

  ReflectorSender *GetRTPSender() { return &fRTPSender; }

  ReflectorSender *GetRTCPSender() { return &fRTCPSender; }

  void SetHasFirstRTCP(bool hasPacket) { fHasFirstRTCPPacket = hasPacket; }

  bool HasFirstRTCP() { return fHasFirstRTCPPacket; }

  void SetFirst_RTCP_RTP_Time(UInt32 time) { fFirst_RTCP_RTP_Time = time; }

  UInt32 GetFirst_RTCP_RTP_Time() { return fFirst_RTCP_RTP_Time; }

  void SetFirst_RTCP_Arrival_Time(SInt64 time) {
    fFirst_RTCP_Arrival_Time = time;
  }

  SInt64 GetFirst_RTCP_Arrival_Time() { return fFirst_RTCP_Arrival_Time; }

  void SetHasFirstRTP(bool hasPacket) { fHasFirstRTPPacket = hasPacket; }

  bool HasFirstRTP() { return fHasFirstRTPPacket; }

  UInt32 GetBufferDelay() { return ReflectorStream::sOverBufferInMsec; }

  UInt32 GetTimeScale() { return fStreamInfo.fTimeScale; }

  UInt64 fPacketCount;

  void SetEnableBuffer(bool enableBuffer) { fEnableBuffer = enableBuffer; }

  bool BufferEnabled() { return fEnableBuffer; }

  inline void UpdateBitRate(SInt64 currentTime);

  static UInt32 sOverBufferInMsec;

  void IncEyeCount() {
    CF::Core::MutexLocker locker(&fBucketMutex);
    fEyeCount++;
  }

  void DecEyeCount() {
    CF::Core::MutexLocker locker(&fBucketMutex);
    fEyeCount--;
  }

  UInt32 GetEyeCount() {
    CF::Core::MutexLocker locker(&fBucketMutex);
    return fEyeCount;
  }

  void SetMyReflectorSession(ReflectorSession *reflector) {
    fMyReflectorSession = reflector;
  }

  ReflectorSession *GetMyReflectorSession() { return fMyReflectorSession; }

 private:

  void DetectStreamFormat();

  // Sends an RTCP receiver report to the broadcast source
  void SendReceiverReport();

  void AllocateBucketArray(UInt32 inNumBuckets);

  SInt32 FindBucket();

  // Reflector sockets, retrieved from the socket pool
  CF::Net::UDPSocketPair *fSockets;

  QTSS_RTPTransportType fTransportType;

  ReflectorSender fRTPSender;
  ReflectorSender fRTCPSender;
  SequenceNumberMap fSequenceNumberMap; //for removing duplicate packets

  // All the necessary info about this stream
  SourceInfo::StreamInfo fStreamInfo;

  enum {
    kStreamFormatUnknown = 0x0000,
    kStreamFormatVideo   = 0x4000,
    kStreamFormatVideoH264,
    kStreamFormatAudio   = 0x8000,
  };

  SInt32 fStreamFormat;

  enum {
    kReceiverReportSize = 16,            // RR(8) + SDES(8)
    kAppSize = 36,
    kMinNumBuckets = 16,
    kBitRateAvgIntervalInMilSecs = 30000 // time between bit-rate averages
  };

  // BUCKET ARRAY
  typedef ReflectorOutput **Bucket;
  Bucket *fOutputArray; // ReflectorOutputs are kept in a 2-dimensional array, "Buckets"

  UInt32 fNumBuckets;        //Number of buckets currently
  UInt32 fNumElements;       //Number of reflector outputs in the array

  //Bucket array can't be modified while we are sending packets.
  CF::Core::Mutex fBucketMutex;

  // RTCP RR information
  char fReceiverReportBuffer[kReceiverReportSize + RTCPSRPacket::kMaxCNameLen + kAppSize]; // contains 3 packets(RR,SDES,APP)
  UInt32 *fEyeLocation; // place in the buffer to write the eye information
  UInt32 fReceiverReportSize;

  // This is the destination address & port for RTCP receiver reports.
  UInt32 fDestRTCPAddr; // remote addr
  UInt16 fDestRTCPPort; // remote port

  // Used for calculating average bit rate
  UInt32 fCurrentBitRate;
  SInt64 fLastBitRateSample;
  std::atomic_uint fBytesSentInThisInterval; // 当前统计区间接收字节数

  // If incoming data is RTSP interleaved
  SInt16 fRTPChannel; //These will be -1 if not set to anything
  SInt16 fRTCPChannel;

  bool fHasFirstRTCPPacket;
  bool fHasFirstRTPPacket;

  bool fEnableBuffer;
  UInt32 fEyeCount;

  UInt32 fFirst_RTCP_RTP_Time;
  SInt64 fFirst_RTCP_Arrival_Time;

  ReflectorSession *fMyReflectorSession;

  static UInt32 sBucketSize;
  static UInt32 sMaxPacketAgeMSec;
  static UInt32 sMaxFuturePacketSec;

  static UInt32 sMaxFuturePacketMSec;
  static UInt32 sOverBufferInSec;
  static UInt32 sBucketDelayInMsec;
  static bool sUsePacketReceiveTime;
  static UInt32 sFirstPacketOffsetMsec;

  static UInt32 sRelocatePacketAgeMSec;

  friend class ReflectorSocket;
  friend class ReflectorSender;

 public:
  CKeyFrameCache *pkeyFrameCache;
};

/**
 * 计算比特率
 */
void ReflectorStream::UpdateBitRate(SInt64 currentTime) {
  if ((fLastBitRateSample + ReflectorStream::kBitRateAvgIntervalInMilSecs) < currentTime) {
    unsigned int intervalBytes = fBytesSentInThisInterval;
    fBytesSentInThisInterval.fetch_sub(intervalBytes); // reduce to 0, accurate for concurrent

    // Multiply by 1000 to convert from milliseconds to seconds, and by 8 to convert from bytes to bits
    Float32 bps = (Float32) (intervalBytes * 8) / (Float32) (currentTime - fLastBitRateSample);
    bps *= 1000;
    fCurrentBitRate = (UInt32) bps;

    // Don't check again for awhile!
    fLastBitRateSample = currentTime;

    DEBUG_LOG(0,
              "ReflectorStream@%p: receive packets. sample time:%" _S64BITARG_ ", bit rate:%" _U32BITARG_ "\n",
              this, fLastBitRateSample, fCurrentBitRate);
  }
}

#endif //_REFLECTOR_SESSION_H_

