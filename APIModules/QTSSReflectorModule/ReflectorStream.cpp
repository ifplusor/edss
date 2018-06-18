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
/**
 * @file ReflectorStream.cpp
 *
 * Implementation of object defined in ReflectorStream.h
 */

#include <CF/Net/Socket/SocketUtils.h>

#include "ReflectorStream.h"
#include "H264Packet.h"
#include "QTSSModuleUtils.h"
#include "RTCPPacket.h"
#include "ReflectorSession.h"

#if DEBUG
#define REFLECTOR_STREAM_DEBUGGING 0
#else
#define REFLECTOR_STREAM_DEBUGGING 0
#endif

using namespace CF;

static ReflectorSocketPool sSocketPool;

// ATTRIBUTES

static QTSS_AttributeID sCantBindReflectorSocketErr = qtssIllegalAttrID;
static QTSS_AttributeID sCantJoinMulticastGroupErr = qtssIllegalAttrID;

// PREFS
static UInt32 sDefaultOverBufferInSec = 1;
static UInt32 sDefaultBucketDelayInMsec = 73;
static bool sDefaultUsePacketReceiveTime = false;
static UInt32 sDefaultMaxFuturePacketTimeSec = 60;
static UInt32 sDefaultFirstPacketOffsetMsec = 500;

UInt32 ReflectorStream::sBucketSize = 16;
UInt32 ReflectorStream::sOverBufferInMsec = 10000; // more or less what the client over buffer will be
UInt32 ReflectorStream::sMaxFuturePacketMSec = 60000; // max packet future time
UInt32 ReflectorStream::sMaxPacketAgeMSec = 20000;

UInt32 ReflectorStream::sMaxFuturePacketSec = 60; // max packet future time
UInt32 ReflectorStream::sOverBufferInSec = 10;
UInt32 ReflectorStream::sBucketDelayInMsec = 73;
bool   ReflectorStream::sUsePacketReceiveTime = false;
UInt32 ReflectorStream::sFirstPacketOffsetMsec = 500;

UInt32 ReflectorStream::sRelocatePacketAgeMSec = 1000;

void ReflectorStream::Register() {
  // Add text messages attributes
  static char *sCantBindReflectorSocket = "QTSSReflectorModuleCantBindReflectorSocket";
  static char *sCantJoinMulticastGroup = "QTSSReflectorModuleCantJoinMulticastGroup";

  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sCantBindReflectorSocket, NULL, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sCantBindReflectorSocket, &sCantBindReflectorSocketErr);

  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sCantJoinMulticastGroup, NULL, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sCantJoinMulticastGroup, &sCantJoinMulticastGroupErr);
}

void ReflectorStream::Initialize(QTSS_ModulePrefsObject inPrefs) {

  QTSSModuleUtils::GetAttribute(inPrefs, "reflector_bucket_offset_delay_msec", qtssAttrDataTypeUInt32,
                                &ReflectorStream::sBucketDelayInMsec, &sDefaultBucketDelayInMsec,
                                sizeof(sBucketDelayInMsec));

  QTSSModuleUtils::GetAttribute(inPrefs, "reflector_buffer_size_sec", qtssAttrDataTypeUInt32,
                                &ReflectorStream::sOverBufferInSec, &sDefaultOverBufferInSec,
                                sizeof(sDefaultOverBufferInSec));

  QTSSModuleUtils::GetAttribute(inPrefs, "reflector_use_in_packet_receive_time", qtssAttrDataTypeBool16,
                                &ReflectorStream::sUsePacketReceiveTime, &sDefaultUsePacketReceiveTime,
                                sizeof(sDefaultUsePacketReceiveTime));

  QTSSModuleUtils::GetAttribute(inPrefs, "reflector_in_packet_max_receive_sec", qtssAttrDataTypeUInt32,
                                &ReflectorStream::sMaxFuturePacketSec, &sDefaultMaxFuturePacketTimeSec,
                                sizeof(sDefaultMaxFuturePacketTimeSec));

  QTSSModuleUtils::GetAttribute(inPrefs, "reflector_rtp_info_offset_msec", qtssAttrDataTypeUInt32,
                                &ReflectorStream::sFirstPacketOffsetMsec, &sDefaultFirstPacketOffsetMsec,
                                sizeof(sDefaultFirstPacketOffsetMsec));

  ReflectorStream::sOverBufferInMsec = sOverBufferInSec * 1000;
  ReflectorStream::sMaxFuturePacketMSec = sMaxFuturePacketSec * 1000;
  ReflectorStream::sMaxPacketAgeMSec = (UInt32) (sOverBufferInMsec * 10); // allow a little time before deleting.
  if (ReflectorStream::sMaxPacketAgeMSec == 0)
    ReflectorStream::sMaxPacketAgeMSec = 10000;
}

void ReflectorStream::GenerateSourceID(SourceInfo::StreamInfo *inInfo, char *ioBuffer) {

  ::memcpy(ioBuffer, &inInfo->fSrcIPAddr, sizeof(inInfo->fSrcIPAddr));
  ::memcpy(&ioBuffer[sizeof(inInfo->fSrcIPAddr)], &inInfo->fPort, sizeof(inInfo->fPort));
}

ReflectorStream::ReflectorStream(SourceInfo::StreamInfo *inInfo)
    : fPacketCount(0),
      fSockets(NULL),
      fRTPSender(NULL, qtssWriteFlagsIsRTP),
      fRTCPSender(NULL, qtssWriteFlagsIsRTCP),
      fOutputArray(NULL),
      fNumBuckets(kMinNumBuckets),
      fNumElements(0),
      fBucketMutex(),

      fDestRTCPAddr(0),
      fDestRTCPPort(0),

      fCurrentBitRate(0),
      fLastBitRateSample(Core::Time::Milliseconds()), // don't calculate our first bit rate until kBitRateAvgIntervalInMilSecs has passed!
      fBytesSentInThisInterval(0),

      fRTPChannel(-1),
      fRTCPChannel(-1),
      fHasFirstRTCPPacket(false),
      fHasFirstRTPPacket(false),
      fEnableBuffer(false),
      fEyeCount(0),
      fFirst_RTCP_RTP_Time(0),
      fFirst_RTCP_Arrival_Time(0),
      fTransportType(qtssRTPTransportTypeTCP),
      fMyReflectorSession(NULL) {

  // 构造函数初始化列表中不能引用this指针
  fRTPSender.fStream = this;
  fRTCPSender.fStream = this;

  fStreamInfo.Copy(*inInfo);

  // ALLOCATE BUCKET ARRAY
  this->AllocateBucketArray(fNumBuckets);

  // WRITE RTCP PACKET
  // 设置 fReceiverReportBuffer、fEyeLocation、fReceiverReportSize

  // write as much of the RTCP RR as is possible right now (most of it never changes)
  UInt32 theSsrc = (UInt32) ::rand();
  char theTempCName[RTCPSRPacket::kMaxCNameLen];
  UInt32 cNameLen = RTCPSRPacket::GetACName(theTempCName);

  // write the RR (just header + ssrc)
  /*
   * RR: Receiver report RTCP packet
   *
   *     0                   1                   2                   3
   *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *    |V=2|P|    RC   |   PT=RR=201   |             length            | header
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *    |                     SSRC of packet sender                     |
   *    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   *    |                 SSRC_1 (SSRC of first source)                 | report
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
   *    | fraction lost |       cumulative number of packets lost       |   1
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *    |           extended highest sequence number received           |
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *    |                      interarrival jitter                      |
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *    |                         last SR (LSR)                         |
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *    |                   delay since last SR (DLSR)                  |
   *    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   *    |                 SSRC_2 (SSRC of second source)                | report
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
   *    :                               ...                             :   2
   *    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   *    |                  profile-specific extensions                  |
   *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *
   * length: 16 bits
   *   The length of this RTCP packet in 32-bit words minus one,
   *   including the header and any padding.
   *
   * @see rfc1889(section 6.3.2)
   *
   */
  UInt32 *theRRWriter = (UInt32 *) &fReceiverReportBuffer[0];
  *theRRWriter++ = htonl(0x80c90001);
  *theRRWriter++ = htonl(theSsrc);
  // TODO: 这是一个空的 RR 包

  // SDES length is the length of the CName, plus 2 32bit words, minus 1
  /*
   * SDES: Source description RTCP packet
   *
   *    0                   1                   2                   3
   *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *   |V=2|P|    SC   |  PT=SDES=202  |             length            | header
   *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   *   |                          SSRC/CSRC_1                          | chunk
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   1
   *   |                           SDES items                          |
   *   |                              ...                              |
   *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   *   |                          SSRC/CSRC_2                          | chunk
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   2
   *   |                           SDES items                          |
   *   |                              ...                              |
   *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   *
   * length: 16 bits
   *   The length of this RTCP packet in 32-bit words minus one,
   *   including the header and any padding.
   *
   * @see rfc1889(section 6.4)
   *
   */
  *theRRWriter++ = htonl(0x81ca0000 + (cNameLen >> 2) + 1);
  *theRRWriter++ = htonl(theSsrc);
  ::memcpy(theRRWriter, theTempCName, cNameLen);
  theRRWriter += cNameLen >> 2;

  // APP packet format, QTSS specific stuff
  /*
   * APP: Application-defined RTCP packet
   *
   *    0                   1                   2                   3
   *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *   |V=2|P| subtype |   PT=APP=204  |             length            |
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *   |                           SSRC/CSRC                           |
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *   |                          name (ASCII)                         |
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *   |                   application-dependent data                 ...
   *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *
   * @see rfc1889(section 6.6)
   *
   */
  *theRRWriter++ = htonl(0x80cc0008);
  *theRRWriter++ = htonl(theSsrc);
  *theRRWriter++ = htonl(FOUR_CHARS_TO_INT('Q', 'T', 'S', 'S'));
  *theRRWriter++ = htonl(0);
  *theRRWriter++ = htonl(0x00000004);
  *theRRWriter++ = htonl(0x6579000c);
  fEyeLocation = theRRWriter;  // 3*4byte

  fReceiverReportSize = kReceiverReportSize + cNameLen + kAppSize;

  // If the source is a multicast, we should send our receiver reports
  // to the multicast address
  if (Net::SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr)) {
    fDestRTCPAddr = fStreamInfo.fDestIPAddr;
    fDestRTCPPort = static_cast<UInt16>(fStreamInfo.fPort + 1);
  }

  pkeyFrameCache = new CKeyFrameCache(MAX_CACHE_SIZE);
}

ReflectorStream::~ReflectorStream() {
  Assert(fNumElements == 0);

  if (fSockets != NULL) {
    //first things first, let's take this stream off the socket's queue
    //of streams. This will basically ensure that no reflecting activity
    //can happen on this stream.
    ((ReflectorSocket *) fSockets->GetSocketA())->RemoveSender(&fRTPSender);
    ((ReflectorSocket *) fSockets->GetSocketB())->RemoveSender(&fRTCPSender);

    //leave the multicast group. Because this socket is shared amongst several
    //potential multicasts, we don't want to remain a member of a stale multicast
    if (Net::SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr)) {
      fSockets->GetSocketA()->LeaveMulticast(fStreamInfo.fDestIPAddr);
      fSockets->GetSocketB()->LeaveMulticast(fStreamInfo.fDestIPAddr);
    }
    //now release the socket pair
    if (qtssRTPTransportTypeUDP == fTransportType)
      sSocketPool.ReleaseUDPSocketPair(fSockets);
    else if (qtssRTPTransportTypeTCP == fTransportType)
      sSocketPool.DestructUDPSocketPair(fSockets);
  }

  // 释放关键帧缓冲区
  if (pkeyFrameCache) {
    delete pkeyFrameCache;
    pkeyFrameCache = NULL;
  }

  //delete every client Bucket
  for (UInt32 y = 0; y < fNumBuckets; y++)
    delete[] fOutputArray[y];
  delete[] fOutputArray;
}

void ReflectorStream::AllocateBucketArray(UInt32 inNumBuckets) {
  Bucket *oldArray = fOutputArray;
  // allocate the 2-dimensional array
  fOutputArray = new Bucket[inNumBuckets];
  for (UInt32 x = 0; x < inNumBuckets; x++) {
    fOutputArray[x] = new ReflectorOutput *[sBucketSize];
    ::memset(fOutputArray[x], 0, sizeof(ReflectorOutput *) * sBucketSize);
  }

  // copy over the old information if there was an old array
  if (oldArray != NULL) {
    Assert(inNumBuckets > fNumBuckets);
    for (UInt32 y = 0; y < fNumBuckets; y++) {
      ::memcpy(fOutputArray[y], oldArray[y], sBucketSize * sizeof(ReflectorOutput *));
      delete[] oldArray[y];
    }
    delete[] oldArray;
  }
  fNumBuckets = inNumBuckets;
}

SInt32 ReflectorStream::AddOutput(ReflectorOutput *inOutput, SInt32 putInThisBucket) {
  Core::MutexLocker locker(&fBucketMutex);

#if DEBUG
  // We should never be adding an output twice to a stream
  for (UInt32 dOne = 0; dOne < fNumBuckets; dOne++)
      for (UInt32 dTwo = 0; dTwo < sBucketSize; dTwo++)
          Assert(fOutputArray[dOne][dTwo] != inOutput);
#endif
  if (inOutput) {
    inOutput->setNewFlag(true);
  }

  // If caller didn't specify a bucket, find a bucket
  if (putInThisBucket < 0)
    putInThisBucket = this->FindBucket();

  Assert(putInThisBucket >= 0);

  if (fNumBuckets <= (UInt32) putInThisBucket)
    this->AllocateBucketArray(putInThisBucket * 2);

  for (UInt32 y = 0; y < sBucketSize; y++) {
    if (fOutputArray[putInThisBucket][y] == NULL) {
      fOutputArray[putInThisBucket][y] = inOutput;
#if REFLECTOR_STREAM_DEBUGGING
      s_printf("Adding new output (0x%lx) to bucket %" _S32BITARG_ ", index %" _S32BITARG_ ",\nnum buckets %li bucketSize: %li \n", (SInt32)inOutput, putInThisBucket, y, (SInt32)fNumBuckets, (SInt32)sBucketSize);
#endif
      fNumElements++;
      return putInThisBucket;
    }
  }
  // There was no empty spot in the specified bucket. Return an error
  return -1;
}

SInt32 ReflectorStream::FindBucket() {
  // If we need more buckets, allocate them.
  if (fNumElements == (sBucketSize * fNumBuckets))
    this->AllocateBucketArray(fNumBuckets * 2);

  // find the first open spot in the array
  for (SInt32 putInThisBucket = 0; (UInt32) putInThisBucket < fNumBuckets; putInThisBucket++) {
    for (UInt32 y = 0; y < sBucketSize; y++)
      if (fOutputArray[putInThisBucket][y] == NULL)
        return putInThisBucket;
  }
  Assert(0);
  return 0;
}

void ReflectorStream::RemoveOutput(ReflectorOutput *inOutput) {
  Core::MutexLocker locker(&fBucketMutex);
  Assert(fNumElements > 0);

  //look at all the indexes in the array
  for (UInt32 x = 0; x < fNumBuckets; x++) {
    for (UInt32 y = 0; y < sBucketSize; y++) {
      //The array may have blank spaces!
      if (fOutputArray[x][y] == inOutput) {
        fOutputArray[x][y] = NULL;//just clear out the pointer

#if REFLECTOR_STREAM_DEBUGGING
        s_printf("Removing output %x from bucket %" _S32BITARG_ ", index %" _S32BITARG_ "\n", inOutput, x, y);
#endif
        fNumElements--;
        return;
      }
    }
  }
  Assert(0);
}

void ReflectorStream::TearDownAllOutputs() {

  Core::MutexLocker locker(&fBucketMutex);

  //look at all the indexes in the array
  for (UInt32 x = 0; x < fNumBuckets; x++) {
    for (UInt32 y = 0; y < sBucketSize; y++) {
      ReflectorOutput *theOutputPtr = fOutputArray[x][y];
      //The array may have blank spaces!
      if (theOutputPtr != NULL) {
        theOutputPtr->TearDown();
#if REFLECTOR_STREAM_DEBUGGING
        s_printf("TearDownAllOutputs Removing output from bucket %" _S32BITARG_ ", index %" _S32BITARG_ "\n", x, y);
#endif
      }
    }
  }
}

QTSS_Error ReflectorStream::
BindSockets(QTSS_StandardRTSP_Params *inParams, UInt32 inReflectorSessionFlags, bool filterState, UInt32 timeout) {
  // If the incoming data is RTSP interleaved, we don't need to do anything here
  if (inReflectorSessionFlags & ReflectorSession::kIsPushSession)
    fStreamInfo.fSetupToReceive = true;

  QTSS_RTSPRequestObject inRequest = NULL;
  if (inParams != NULL)
    inRequest = inParams->inRTSPRequest;

  // Set the transport Type a Broadcaster
  if (inParams != NULL) {
    UInt32 theLen = sizeof(fTransportType);
    (void) QTSS_GetValue(inParams->inRTSPRequest, qtssRTSPReqTransportType, 0, (void *) &fTransportType, &theLen);
  }

  // get a pair of sockets. The socket must be bound on INADDR_ANY because we don't know
  // which interface has access to this broadcast. If there is a source IP address
  // specified by the source info, we can use that to demultiplex separate broadcasts on
  // the same port. If the src IP addr is 0, we cannot do this and must dedicate 1 port per
  // broadcast

  // changing INADDR_ANY to fStreamInfo.fDestIPAddr to deal with NATs (need to track this change though)
  // change submitted by denis@berlin.ccc.de

  // 这里是检查 sdp 文件所指定的目的地址是否为多播地址
  bool isMulticastDest = (Net::SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr));

  // Babosa修改:当RTSP TCP推送的时候,直接创建SocketPair,不经过UDPSocketPool
  if (qtssRTPTransportTypeTCP == fTransportType) {
    fSockets = sSocketPool.ConstructUDPSocketPair();
  } else {
    // NOTE: 对于指定了 IP、Port 的情况, QTSServer::SetupUDPSockets 创建的 socket 对显然无法满足要求,
    //   所以这里会再次创建 socket 对。
    //   fStreamInfo.fDestIPAddr 是从 sdp 的 c 行解析, fStreamInfo.fPort 是从 m 行解析
    if (isMulticastDest) {
      fSockets = sSocketPool.GetUDPSocketPair(INADDR_ANY, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
    } else {
      fSockets = sSocketPool.GetUDPSocketPair(fStreamInfo.fDestIPAddr, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
    }

    if ((fSockets == NULL) && fStreamInfo.fSetupToReceive) {
      fStreamInfo.fPort = 0;
      if (isMulticastDest) {
        fSockets = sSocketPool.GetUDPSocketPair(INADDR_ANY, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
      } else {
        fSockets = sSocketPool.GetUDPSocketPair(fStreamInfo.fDestIPAddr, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
      }
    }
  }

  if (fSockets == NULL)
    return QTSSModuleUtils::SendErrorResponse(inRequest, qtssServerInternal, sCantBindReflectorSocketErr);

  // If we know the source IP address of this broadcast, we can demux incoming traffic
  // on the same port by that source IP address. If we don't know the source IP addr,
  // it is impossible for us to demux, and therefore we shouldn't allow multiple
  // broadcasts on the same port.
  if (((ReflectorSocket *) fSockets->GetSocketA())->HasSender() && (fStreamInfo.fSrcIPAddr == 0))
    return QTSSModuleUtils::SendErrorResponse(inRequest, qtssServerInternal, sCantBindReflectorSocketErr);

  // also put this stream onto the socket's queue of streams
  ((ReflectorSocket *) fSockets->GetSocketA())->AddSender(&fRTPSender);
  ((ReflectorSocket *) fSockets->GetSocketB())->AddSender(&fRTCPSender);

  // A broadcaster is setting up a UDP session so let the sockets update the session
  if (fStreamInfo.fSetupToReceive && qtssRTPTransportTypeUDP == fTransportType && inParams != NULL) {
    ((ReflectorSocket *) fSockets->GetSocketA())->AddBroadcasterSession(inParams->inClientSession);
    ((ReflectorSocket *) fSockets->GetSocketB())->AddBroadcasterSession(inParams->inClientSession);
  }

  ((ReflectorSocket *) fSockets->GetSocketA())->SetSSRCFilter(filterState, timeout);
  ((ReflectorSocket *) fSockets->GetSocketB())->SetSSRCFilter(filterState, timeout);

#if 1
  // Always set the Rcv buf size for the sockets. This is important because the
  // server is going to be getting many packets on these sockets.
  if (qtssRTPTransportTypeUDP == fTransportType) {
    fSockets->GetSocketA()->SetSocketRcvBufSize(1024 * 1024);
    fSockets->GetSocketB()->SetSocketRcvBufSize(1024 * 1024);
  }
#endif

  // If the broadcaster is sending RTP directly to us, we don't
  // need to join a multicast group because we're not using multicast
  if (isMulticastDest) {
    QTSS_Error err = fSockets->GetSocketA()->JoinMulticast(fStreamInfo.fDestIPAddr);
    if (err == QTSS_NoErr)
      err = fSockets->GetSocketB()->JoinMulticast(fStreamInfo.fDestIPAddr);
    // If we get an error when setting the TTL, this isn't too important (TTL on
    // these sockets is only useful for RTCP RRs.
    if (err == QTSS_NoErr)
      (void) fSockets->GetSocketA()->SetTtl(fStreamInfo.fTimeToLive);
    if (err == QTSS_NoErr)
      (void) fSockets->GetSocketB()->SetTtl(fStreamInfo.fTimeToLive);
    if (err != QTSS_NoErr)
      return QTSSModuleUtils::SendErrorResponse(inRequest, qtssServerInternal, sCantJoinMulticastGroupErr);
  }

  // If the port is 0, update the port to be the actual port value
  fStreamInfo.fPort = fSockets->GetSocketA()->GetLocalPort();

  // finally, register these sockets for events
  if (qtssRTPTransportTypeUDP == fTransportType) {
    fSockets->GetSocketA()->RequestEvent(EV_RE);
    fSockets->GetSocketB()->RequestEvent(EV_RE);
  }

  return QTSS_NoErr;
}

void ReflectorStream::SendReceiverReport() {
  // Check to see if our destination RTCP addr & port are setup.
  // They may not be if the source is unicast and we haven't gotten any incoming packets yet
  if (fDestRTCPAddr == 0) return;

  // eye count field for app packet
  UInt32 theEyeCount = this->GetEyeCount();
  UInt32 *theEyeWriter = fEyeLocation;
  *theEyeWriter++ = htonl(theEyeCount) & 0x7fffffff;
  *theEyeWriter++ = htonl(theEyeCount) & 0x7fffffff;
  *theEyeWriter   = htonl(0) & 0x7fffffff;

  // send the 3 packets(RR,SDES,APP) to the multicast RTCP addr & port for this stream
  (void) fSockets->GetSocketB()->SendTo(fDestRTCPAddr, fDestRTCPPort, fReceiverReportBuffer, fReceiverReportSize);
}

void ReflectorStream::PushPacket(char *packet, UInt32 packetLen, bool isRTCP) {
  if (packetLen <= 0) return;;

  ReflectorSocket *theSocket = nullptr;
  if (isRTCP) {
    theSocket = (ReflectorSocket *) fSockets->GetSocketB();
  } else {
    theSocket = (ReflectorSocket *) fSockets->GetSocketA();
  }

  //s_printf("ReflectorStream::PushPacket %s packetlen = %"   _U32BITARG_   "\n", isRTCP ? "RTCP" : "RTP", packetLen);
  ReflectorPacket *thePacket = theSocket->GetPacket();
  if (thePacket == nullptr) {
    //s_printf("ReflectorStream::PushPacket %s GetPacket() is NULL\n", isRTCP ? "RTCP" : "RTP");
    return;
  }

  Core::MutexLocker locker(theSocket->GetDemuxer()->GetMutex());
  thePacket->SetPacketData(packet, packetLen, isRTCP);
  theSocket->ProcessPacket(Core::Time::Milliseconds(), thePacket, 0, 0);
  theSocket->Signal(Thread::Task::kIdleEvent);
}

ReflectorSender::ReflectorSender(ReflectorStream *inStream, UInt32 inWriteFlag)
    : fStream(inStream),
      fWriteFlag(inWriteFlag),
      fFirstNewPacketInQueue(NULL),
      fFirstPacketInQueueForNewOutput(NULL),
      fKeyFrameStartPacketElementPointer(NULL),
      fHasNewPackets(false),
      fNextTimeToRun(0),
      fLastRRTime(0),
      fSocketQueueElem() {
  fSocketQueueElem.SetEnclosingObject(this);
}

ReflectorSender::~ReflectorSender() {
  //d equeue and delete every buffer
  while (fPacketQueue.GetLength() > 0) {
    ReflectorPacket *packet = (ReflectorPacket *) fPacketQueue.DeQueue()->GetEnclosingObject();
    delete packet;
  }
}

/**
 * 检测是否有新的数据包，或到达唤醒时间
 */
bool ReflectorSender::ShouldReflectNow(const SInt64 &inCurrentTime, SInt64 *ioWakeupTime) {
  Assert(ioWakeupTime != NULL);
  // check to make sure there actually is work to do for this stream.
  if ((!fHasNewPackets) && ((fNextTimeToRun == 0) || (inCurrentTime < fNextTimeToRun))) {
    // We don't need to do work right now, but
    // this stream must still communicate when it needs to be woken up next
    SInt64 theWakeupTime = fNextTimeToRun + inCurrentTime;
    //s_printf("ReflectorSender::ShouldReflectNow theWakeupTime=%qd newWakeUpTime=%qd  ioWakepTime=%qd\n", theWakeupTime, fNextTimeToRun + inCurrentTime,*ioWakeupTime);
    if ((fNextTimeToRun > 0) && (theWakeupTime < *ioWakeupTime))
      *ioWakeupTime = theWakeupTime;
    return false;
  }
  return true;
}

UInt32 ReflectorSender::GetOldestPacketRTPTime(bool *foundPtr) {
  if (foundPtr != NULL)
    *foundPtr = false;
  Core::MutexLocker locker(&fStream->fBucketMutex);
  QueueElem *packetElem = this->GetClientBufferStartPacket();
  if (packetElem == NULL)
    return 0;

  ReflectorPacket
      *thePacket = (ReflectorPacket *) packetElem->GetEnclosingObject();
  if (thePacket == NULL)
    return 0;

  if (foundPtr != NULL)
    *foundPtr = true;

  return thePacket->GetPacketRTPTime();
}

UInt16 ReflectorSender::GetFirstPacketRTPSeqNum(bool *foundPtr) {
  if (foundPtr != NULL)
    *foundPtr = false;

  UInt16 resultSeqNum = 0;
  Core::MutexLocker locker(&fStream->fBucketMutex);
  QueueElem *packetElem = this->GetClientBufferStartPacket();

  if (packetElem == NULL)
    return 0;

  ReflectorPacket
      *thePacket = (ReflectorPacket *) packetElem->GetEnclosingObject();
  if (thePacket == NULL)
    return 0;

  if (foundPtr != NULL)
    *foundPtr = true;

  resultSeqNum = thePacket->GetPacketRTPSeqNum();

  return resultSeqNum;
}

QueueElem *ReflectorSender::GetClientBufferNextPacketTime(UInt32 inRTPTime) {

  QueueIter qIter(&fPacketQueue);// start at oldest packet in q
  QueueElem *requestedPacket = NULL;
  QueueElem *elem = NULL;

  while (!qIter.IsDone()) { // start at oldest packet in q
    elem = qIter.GetCurrent();

    if (requestedPacket == NULL)
      requestedPacket = elem;

    if (requestedPacket == NULL)
      break;

    ReflectorPacket *thePacket = (ReflectorPacket *) elem->GetEnclosingObject();
    Assert(thePacket);

    if (thePacket->GetPacketRTPTime() > inRTPTime) {
      requestedPacket =
          elem; // return the first packet we have that has a later time
      break; // found the packet we need: done processing
    }
    qIter.Next();

  }

  return requestedPacket;
}

bool ReflectorSender::GetFirstRTPTimePacket(UInt16 *outSeqNumPtr, UInt32 *outRTPTimePtr, SInt64 *outArrivalTimePtr) {
  Core::MutexLocker locker(&fStream->fBucketMutex);
  QueueElem *packetElem =
      this->GetClientBufferStartPacketOffset(ReflectorStream::sFirstPacketOffsetMsec);

  if (packetElem == NULL)
    return false;

  ReflectorPacket *thePacket = (ReflectorPacket *)
      packetElem->GetEnclosingObject();
  if (thePacket == NULL)
    return false;

  packetElem = GetClientBufferNextPacketTime(thePacket->GetPacketRTPTime());
  if (packetElem == NULL)
    return false;

  thePacket = (ReflectorPacket *) packetElem->GetEnclosingObject();
  if (thePacket == NULL)
    return false;

  if (outSeqNumPtr)
    *outSeqNumPtr = thePacket->GetPacketRTPSeqNum();

  if (outRTPTimePtr)
    *outRTPTimePtr = thePacket->GetPacketRTPTime();

  if (outArrivalTimePtr)
    *outArrivalTimePtr = thePacket->fTimeArrived;

  return true;
}

bool ReflectorSender::GetFirstPacketInfo(UInt16 *outSeqNumPtr, UInt32 *outRTPTimePtr, SInt64 *outArrivalTimePtr) {
  Core::MutexLocker locker(&fStream->fBucketMutex);
  QueueElem *packetElem = this->GetClientBufferStartPacketOffset(ReflectorStream::sFirstPacketOffsetMsec);

  if (packetElem == NULL) return false;

  auto *thePacket = (ReflectorPacket *) packetElem->GetEnclosingObject();
  if (thePacket == NULL) return false;

  if (outSeqNumPtr) *outSeqNumPtr = thePacket->GetPacketRTPSeqNum();
  if (outRTPTimePtr) *outRTPTimePtr = thePacket->GetPacketRTPTime();
  if (outArrivalTimePtr) *outArrivalTimePtr = thePacket->fTimeArrived;

  thePacket->fNeededByOutput = true;

  return true;
}

#if REFLECTOR_STREAM_DEBUGGING
static UInt16 DGetPacketSeqNumber(StrPtrLen* inPacket)
{
    if (inPacket->Len < 4)
        return 0;

    //The RTP seq number is the second short of the packet
    UInt16* seqNumPtr = (UInt16*)inPacket->Ptr;
    return ntohs(seqNumPtr[1]);
}



#endif

void ReflectorSender::ReflectRelayPackets(SInt64 *ioWakeupTime, Queue *inFreeQueue) {
  //Most of this code is useless i.e. buckets and bookmarks. This code will get cleaned up eventually

  //printf("ReflectorSender::ReflectPackets %qd %qd\n",*ioWakeupTime,fNextTimeToRun);
#if DEBUG
  Assert(ioWakeupTime != NULL);
#endif
#if REFLECTOR_STREAM_DEBUGGING > 2
  bool	printQueueLenOnExit = false;
#endif

  SInt64 currentTime = Core::Time::Milliseconds();

  //make sure to reset these state variables
  fHasNewPackets = false;
  fNextTimeToRun = 1000;    // init to 1 secs

  //determine if we need to send a receiver report to the multicast source
  if ((fWriteFlag == qtssWriteFlagsIsRTCP) && (currentTime > (fLastRRTime + kRRInterval))) {
    fLastRRTime = currentTime;
    fStream->SendReceiverReport();
#if REFLECTOR_STREAM_DEBUGGING > 2
    printQueueLenOnExit = true;
    printf("fPacketQueue len %li\n", (SInt32)fPacketQueue.GetLength());
#endif
  }

  //the rest of this function must be atomic write the ReflectorSession, because
  //it involves iterating through the RTPSession array, which isn't thread safe
  Core::MutexLocker locker(&fStream->fBucketMutex);

  // Check to see if we should update the session's bitrate average
  if ((fStream->fLastBitRateSample + ReflectorStream::kBitRateAvgIntervalInMilSecs) < currentTime) {
    unsigned int intervalBytes = fStream->fBytesSentInThisInterval;
    //(void)atomic_sub(&fStream->fBytesSentInThisInterval, intervalBytes);
    fStream->fBytesSentInThisInterval.fetch_sub(intervalBytes);

    // Multiply by 1000 to convert from milliseconds to seconds, and by 8 to convert from bytes to bits
    Float32 bps = (Float32) (intervalBytes * 8) / (Float32) (currentTime - fStream->fLastBitRateSample);
    bps *= 1000;
    fStream->fCurrentBitRate = (UInt32) bps;

    // Don't check again for awhile!
    fStream->fLastBitRateSample = currentTime;
  }

  for (UInt32 bucketIndex = 0; bucketIndex < fStream->fNumBuckets; bucketIndex++) {
    for (UInt32 bucketMemberIndex = 0; bucketMemberIndex < fStream->sBucketSize; bucketMemberIndex++) {
      ReflectorOutput *theOutput = fStream->fOutputArray[bucketIndex][bucketMemberIndex];

      if (theOutput != NULL) {
        SInt32 availBookmarksPosition = -1;    // -1 == invalid position
        QueueElem *packetElem = NULL;
        UInt32 curBookmark = 0;

        Assert(curBookmark < theOutput->fNumBookmarks);

        // see if we've bookmarked a held packet for this Sender in this Output
        while (curBookmark < theOutput->fNumBookmarks) {
          QueueElem *bookmarkedElem = theOutput->fBookmarkedPacketsElemsArray[curBookmark];

          if (bookmarkedElem) {   // there may be holes in this array
            if (bookmarkedElem->IsMember(fPacketQueue)) {
              // this packet was previously bookmarked for this specific queue
              // remove if from the bookmark list and use it
              // to jump ahead into the Sender's over all packet queue
              theOutput->fBookmarkedPacketsElemsArray[curBookmark] = NULL;
              availBookmarksPosition = curBookmark;
              packetElem = bookmarkedElem;
              break;
            }

          } else {
            availBookmarksPosition = curBookmark;
          }

          curBookmark++;

        }

        Assert(availBookmarksPosition != -1);

#if REFLECTOR_STREAM_DEBUGGING > 1
        if (packetElem)	// show 'em what we got johnny
        {
            ReflectorPacket* 	thePacket = (ReflectorPacket*)packetElem->GetEnclosingObject();
            printf("Bookmarked packet time: %li, packetSeq %i\n", (SInt32)thePacket->fTimeArrived, DGetPacketSeqNumber(&thePacket->fPacketPtr));
        }
#endif

        // the output did not have a bookmarked packet if it's own
        // so show it the first new packet we have in this sender.
        // ( since TCP flow control may delay the sending of packets, this may not
        // be the same as the first packet in the queue
        if (packetElem == NULL) {
          packetElem = fFirstNewPacketInQueue;

#if REFLECTOR_STREAM_DEBUGGING > 1
          if (packetElem)	// show 'em what we got johnny
          {
              ReflectorPacket* 	thePacket = (ReflectorPacket*)packetElem->GetEnclosingObject();
              printf("1st new packet from Sender sess 0x%lx time: %li, packetSeq %i\n", (SInt32)theOutput, (SInt32)thePacket->fTimeArrived, DGetPacketSeqNumber(&thePacket->fPacketPtr));
          }
          else
              printf("no new packets\n");
#endif
        }

        QueueIter qIter(&fPacketQueue, packetElem);  // starts from beginning if packetElem == NULL, else from packetElem

        bool dodBookmarkPacket = false;

        while (!qIter.IsDone()) {
          packetElem = qIter.GetCurrent();

          ReflectorPacket *thePacket = (ReflectorPacket *) packetElem->GetEnclosingObject();
          QTSS_Error err = QTSS_NoErr;

#if REFLECTOR_STREAM_DEBUGGING > 2
          printf("packet time: %li, packetSeq %i\n", (SInt32)thePacket->fTimeArrived, DGetPacketSeqNumber(&thePacket->fPacketPtr));
#endif

          // once we see a packet we cant' send, we need to stop trying
          // during this pass mark remaining as still needed
          if (!dodBookmarkPacket) {
            SInt64 packetLateness = currentTime - thePacket->fTimeArrived - (ReflectorStream::sBucketDelayInMsec * (SInt64) bucketIndex);
            // packetLateness measures how late this packet it after being corrected for the bucket delay

#if REFLECTOR_STREAM_DEBUGGING > 2
            printf("packetLateness %li, seq# %li\n", (SInt32)packetLateness, (SInt32)DGetPacketSeqNumber(&thePacket->fPacketPtr));
#endif

            SInt64 timeToSendPacket = -1;
            err = theOutput->WritePacket(&thePacket->fPacketPtr, fStream, fWriteFlag, packetLateness, &timeToSendPacket, NULL, NULL, false);

            if (err == QTSS_WouldBlock) {
#if REFLECTOR_STREAM_DEBUGGING > 2
              printf("EAGAIN bookmark: %li, packetSeq %i\n", (SInt32)packetLateness, DGetPacketSeqNumber(&thePacket->fPacketPtr));
#endif
              // tag it and bookmark it
              thePacket->fNeededByOutput = true;

              Assert(availBookmarksPosition != -1);
              if (availBookmarksPosition != -1)
                theOutput->fBookmarkedPacketsElemsArray[availBookmarksPosition] = packetElem;

              dodBookmarkPacket = true;

              // call us again in # ms to retry on an EAGAIN
              if ((timeToSendPacket > 0) && (fNextTimeToRun > timeToSendPacket))
                fNextTimeToRun = timeToSendPacket;
              if (timeToSendPacket == -1)
                this->SetNextTimeToRun( 5); // keep in synch with delay on would block for on-demand lower is better for high-bit rate movies.

            }
          } else {
            if (thePacket->fNeededByOutput)    // optimization: if the packet is already marked, another Output has been through this already
              break;
            thePacket->fNeededByOutput = true;
          }

          qIter.Next();
        }

      }
    }
  }

  // reset our first new packet bookmark
  fFirstNewPacketInQueue = NULL;

  // iterate one more through the senders queue to clear out
  // the unneeded packets
  QueueIter removeIter(&fPacketQueue);
  while (!removeIter.IsDone()) {
    QueueElem *elem = removeIter.GetCurrent();
    Assert(elem);

    //at this point, move onto the next queue element, because we may be altering
    //the queue itself in the code below
    removeIter.Next();

    ReflectorPacket *thePacket = (ReflectorPacket *) elem->GetEnclosingObject();
    Assert(thePacket);

    if (thePacket->fNeededByOutput == false) {
      thePacket->fNeededByOutput = true;
      fPacketQueue.Remove(elem);
      inFreeQueue->EnQueue(elem);
    } else {   // reset for next call to ReflectPackets
      thePacket->fNeededByOutput = false;
    }
  }

  //Don't forget that the caller also wants to know when we next want to run
  if (*ioWakeupTime == 0)
    *ioWakeupTime = fNextTimeToRun;
  else if ((fNextTimeToRun > 0) && (*ioWakeupTime > fNextTimeToRun))
    *ioWakeupTime = fNextTimeToRun;
  // exit with fNextTimeToRun in real time, not relative time.
  fNextTimeToRun += currentTime;

#if REFLECTOR_STREAM_DEBUGGING > 2
  if (printQueueLenOnExit)
      printf("EXIT fPacketQueue len %li\n", (SInt32)fPacketQueue.GetLength());
#endif
}


/**
 *  There are n ReflectorSender's for n output streams per presentation.
 *
 *  Each sender is associated with an array of ReflectorOutput's. Each
 *  output represents a client connection.  Each output has # RTPStream's.
 *
 *  When we write a packet to the ReflectorOutput he matches it's payload
 *  to one of his streams and sends it there.
 *
 *  To smooth the bandwitdth (server, not user) requirements of the reflected streams, the Sender
 *  groups the ReflectorOutput's into buckets.  The input streams are reflected to
 *  each bucket progressively later in time.  So rather than send a single packet
 *  to say 1000 clients all at once, we send it to just the first 16, then then next 16
 *  100 ms later and so on.
 *
 * @param ioWakeupTime relative time to call us again in MSec
 * @param inFreeQueue queue of free packets
 */
void ReflectorSender::ReflectPackets(SInt64 *ioWakeupTime, Queue *inFreeQueue) {
  // 在 reflectorSession::FindOrCreateSession 函数里,调用了 SetHasBufferedStreams(true)
  if (!fStream->BufferEnabled()) { // Call old routine for relays; they don't want buffering.
    this->ReflectRelayPackets(ioWakeupTime, inFreeQueue);
    return;
  }

  SInt64 currentTime = Core::Time::Milliseconds();

  // make sure to reset these state variables
  fHasNewPackets = false;

  fNextTimeToRun = 1000; // init to 1 secs

  // we set fWriteFlag in ReflectorStream constructor
  //   - fRTPSender: qtssWriteFlagsIsRTP
  //   - fRTCPSender: qtssWriteFlagsIsRTCP
  if (fWriteFlag == qtssWriteFlagsIsRTCP)
    fNextTimeToRun = 1000;

  // determine if we need to send a receiver report to the multicast source
  if ((fWriteFlag == qtssWriteFlagsIsRTCP) && (currentTime > (fLastRRTime + kRRInterval))) {
    fLastRRTime = currentTime;
    fStream->SendReceiverReport();
  }

  // the rest of this function must be atomic write the ReflectorSession, because
  // it involves iterating through the RTPSession array, which isn't thread safe
  Core::MutexLocker locker(&fStream->fBucketMutex);

  // Check to see if we should update the session's bitrate average
  fStream->UpdateBitRate(currentTime);

  // 视频数据流，最好直接定位到第一个关键帧起始包这样出视频的时间会更快一些
  if (fKeyFrameStartPacketElementPointer) {
    fFirstPacketInQueueForNewOutput = fKeyFrameStartPacketElementPointer;
  } else {
    // where to start new clients in the q
    fFirstPacketInQueueForNewOutput = this->GetClientBufferStartPacketOffset(0);
  }

#if (0) //test code 
  if (NULL != fFirstPacketInQueueForNewOutput)
      printf("ReflectorSender::ReflectPackets SET first packet fFirstPacketInQueueForNewOutput %d \n", DGetPacketSeqNumber(&((ReflectorPacket*)(fFirstPacketInQueueForNewOutput->GetEnclosingObject()))->fPacketPtr));

  ReflectorPacket* thePacket = NULL;
  if (fFirstPacketInQueueForNewOutput != NULL)
      thePacket = (ReflectorPacket*)fFirstPacketInQueueForNewOutput->GetEnclosingObject();
  if (thePacket == NULL) {
      printf("fFirstPacketInQueueForNewOutput is NULL \n");
  }
#endif

  bool firstPacket = false;

  // 我们在 QTSSReflectorModule::DoSetup 里面看到,对于一个 ReflectorSession 的每一个
  // ReflectorStream,都调用了 AddOutput 添加了一个 RTPSessionOutput 对象。
  // 在开启 n 个窗口同时播放同一个 sdp 文件的情况下,会有 n 个 theOutput 对应 n 个 RTPStream,
  // 依次通过这 n 个 theOutput 发送 RTP 数据。
  for (UInt32 bucketIndex = 0; bucketIndex < fStream->fNumBuckets; bucketIndex++) {
    for (UInt32 bucketMemberIndex = 0; bucketMemberIndex < ReflectorStream::sBucketSize; bucketMemberIndex++) {
      ReflectorOutput *theOutput = fStream->fOutputArray[bucketIndex][bucketMemberIndex];
      if (theOutput == NULL || !theOutput->IsPlaying()) continue;

      {
        Core::MutexLocker locker(&theOutput->fMutex);

        // 返回既在 fBookmarkedPacketsElemsArray 数组同时属于 fPacketQueue 的成员
        QueueElem *packetElem = theOutput->GetBookMarkedPacket(&fPacketQueue);
        if (packetElem == NULL) { // should only be a new output
          // everybody starts at the oldest packet in the buffer delay or uses a bookmark
          packetElem = fFirstPacketInQueueForNewOutput;
          firstPacket = true;
          theOutput->setNewFlag(false);
        } else {
          firstPacket = false;
        }

        // sBucketDelayInMsec 对应于配置文件中的 reflector_bucket_offset_delay_msec,缺省值为 73.
        SInt64 bucketDelay = ReflectorStream::sBucketDelayInMsec * (SInt64) bucketIndex;
        packetElem = this->SendPacketsToOutput(theOutput, packetElem, currentTime, bucketDelay, firstPacket);
        if (packetElem) {
          QueueElem *newElem = NeedRelocateBookMark(packetElem);

          auto *thePacket = (ReflectorPacket *) newElem->GetEnclosingObject();
          thePacket->fNeededByOutput = true;               // flag to prevent removal in RemoveOldPackets
          (void) theOutput->SetBookMarkPacket(newElem);    // store a reference to the packet
        }
      }
    }
  }

  this->RemoveOldPackets(inFreeQueue);
  fFirstNewPacketInQueue = NULL;

  // Don't forget that the caller also wants to know when we next want to run
  // ReflectorSocket::Run 根据 *ioWakeupTime 来决定 idelTimer 的值。
  if (*ioWakeupTime == 0)
    *ioWakeupTime = fNextTimeToRun;
  else if ((fNextTimeToRun > 0) && (*ioWakeupTime > fNextTimeToRun))
    *ioWakeupTime = fNextTimeToRun;

  // exit with fNextTimeToRun in real time, not relative time.
  fNextTimeToRun += currentTime;

  // s_printf("SetNextTimeToRun fNextTimeToRun=%qd + currentTime=%qd\n", fNextTimeToRun, currentTime);
  // s_printf("ReflectorSender::ReflectPackets *ioWakeupTime = %qd\n", *ioWakeupTime);

}

QueueElem *ReflectorSender::SendPacketsToOutput(ReflectorOutput *theOutput, QueueElem *currentPacket,
                                                SInt64 currentTime, SInt64 bucketDelay, bool firstPacket) {
  QueueElem *lastPacket = currentPacket;
  QueueIter qIter(&fPacketQueue, currentPacket); // starts from beginning if currentPacket == NULL, else from currentPacket

  UInt32 count = 0;
  QTSS_Error err = QTSS_NoErr;
  while (!qIter.IsDone()) {
    currentPacket = qIter.GetCurrent();
    lastPacket = currentPacket;

    auto *thePacket = (ReflectorPacket *) currentPacket->GetEnclosingObject();
    if (thePacket == NULL) {
      // TODO(james): crash for NULL pointer
      Assert(false);
    }
    SInt64 packetLateness = bucketDelay;
    SInt64 timeToSendPacket = -1;

    //printf("packetLateness %qd, seq# %li\n", packetLateness, (SInt32) DGetPacketSeqNumber( &thePacket->fPacketPtr ) );

    // 实际上是调用 RTPSessionOutput::WritePacket
    err = theOutput->WritePacket(&thePacket->fPacketPtr, fStream, fWriteFlag, packetLateness, &timeToSendPacket,
                                 &thePacket->fStreamCountID, &thePacket->fTimeArrived, firstPacket);

    if (err == QTSS_WouldBlock) { // call us again in # ms to retry on an EAGAIN

      if ((timeToSendPacket > 0) && ((fNextTimeToRun + currentTime) > timeToSendPacket)) // blocked but we are scheduled to wake up later
        fNextTimeToRun = timeToSendPacket - currentTime;

      if (theOutput->fLastIntervalMilliSec < 5)
        theOutput->fLastIntervalMilliSec = 5;

      if (timeToSendPacket < 0) { // blocked and we are behind
        //s_printf("fNextTimeToRun = theOutput->fLastIntervalMilliSec=%qd;\n", theOutput->fLastIntervalMilliSec); // Use the last packet interval
        this->SetNextTimeToRun(theOutput->fLastIntervalMilliSec);
      }

      if (fNextTimeToRun > 100) { // don't wait that long
        //s_printf("fNextTimeToRun = %qd now 100;\n", fNextTimeToRun);
        this->SetNextTimeToRun(100);
      }

      if (fNextTimeToRun < 5) { // wait longer
        //s_printf("fNextTimeToRun = 5;\n");
        this->SetNextTimeToRun(5);
      }

      if (theOutput->fLastIntervalMilliSec >= 100) // allow up to 1 second max -- allow some time for the socket to clear and don't go into a tight loop if the client is gone.
        theOutput->fLastIntervalMilliSec = 100;
      else
        theOutput->fLastIntervalMilliSec *= 2; // scale upwards over time

      //s_printf ( "Blocked ReflectorSender::SendPacketsToOutput timeToSendPacket=%qd fLastIntervalMilliSec=%qd fNextTimeToRun=%qd \n", timeToSendPacket, theOutput->fLastIntervalMilliSec, fNextTimeToRun);

      break;
    }

    count++;
    qIter.Next();
  }

  // 如果 WritePacket 返回为 QTSS_WouldBlock,则 lastPacket 不为空。
  return lastPacket;
}

/**
 * 查找处于缓存期内，最早到达的数据包
 *
 * @param offsetMsec 缓冲期缩短偏移
 */
QueueElem *ReflectorSender::GetClientBufferStartPacketOffset(SInt64 offsetMsec, bool needKeyFrameFirstPacket) {
  QueueIter qIter(&fPacketQueue);// start at oldest packet in q
  SInt64 theCurrentTime = Core::Time::Milliseconds();
  QueueElem *oldestPacketInClientBufferTime = NULL;

  // 这里的 sOverBufferInSec 对应配置文件中的 reflector_buffer_size_sec*1000
  if (offsetMsec > ReflectorStream::sOverBufferInMsec)
    offsetMsec = ReflectorStream::sOverBufferInMsec;

  while (!qIter.IsDone()) { // start at oldest packet in q
    QueueElem *elem = qIter.GetCurrent();
    Assert(elem);
    qIter.Next();

    auto *thePacket = (ReflectorPacket *) elem->GetEnclosingObject();
    Assert(thePacket);

    SInt64 packetDelay = theCurrentTime - thePacket->fTimeArrived;
    if (packetDelay <= (ReflectorStream::sOverBufferInMsec - offsetMsec)) {
      oldestPacketInClientBufferTime = &thePacket->fQueueElem;
      break; // found the packet we need: done processing
    }
  }

  return oldestPacketInClientBufferTime;
}

void ReflectorSender::RemoveOldPackets(Queue *inFreeQueue) {
  // Iterate through the senders queue to clear out packets
  // Start at the oldest packet and walk forward to the newest packet
  //
  QueueIter removeIter(&fPacketQueue);
  SInt64 theCurrentTime = Core::Time::Milliseconds();

  // sMaxPacketAgeMSec 对应于配置文件中 reflector_buffer_size_sec*10000, 缺省为 10s
  SInt64 currentMaxPacketDelay = ReflectorStream::sMaxPacketAgeMSec;

  // walk q and remove packets that are too old
  while (!removeIter.IsDone()) {
    QueueElem *elem = removeIter.GetCurrent();
    Assert(elem);

    // at this point, move onto the next queue element, because we may be altering
    // the queue itself in the code below
    removeIter.Next();

    auto *thePacket = (ReflectorPacket *) elem->GetEnclosingObject();
    Assert(thePacket);
    //printf("ReflectorSender::RemoveOldPackets Packet %d in queue is %qd milliseconds old\n", DGetPacketSeqNumber( &thePacket->fPacketPtr ) ,theCurrentTime - thePacket->fTimeArrived);

    SInt64 packetDelay = theCurrentTime - thePacket->fTimeArrived;

    // delete based on late tolerance and whether a client is blocked on the packet
    if (!thePacket->fNeededByOutput && packetDelay > currentMaxPacketDelay) {
      // not needed and older than our required buffer
      thePacket->Reset();
      fPacketQueue.Remove(elem);
      inFreeQueue->EnQueue(elem);
    } else {
      // we want to keep all of these but we should reset the ones that should be aged out unless marked
      // as need the next time through reflect packets.

      if (fKeyFrameStartPacketElementPointer) { // 关键帧不能被清理
        auto *keyPacket = (ReflectorPacket *) (fKeyFrameStartPacketElementPointer->GetEnclosingObject());
        if (keyPacket == thePacket) break;
      }

      // TODO(james):
      thePacket->fNeededByOutput = false; // mark not needed.. will be set next time through reflect packets

      // this packet is going to be kept around as well as the ones that follow.
      if (packetDelay <= currentMaxPacketDelay) break;
    }
  }
}

/**
 * if current packet over max packetAgeTime, we need relocate the BookMark to
 * the new fKeyFrameStartPacketElementPointer
 */
QueueElem *ReflectorSender::NeedRelocateBookMark(QueueElem *elem) {
  Assert(elem);

  // 1. 判断当前Packet是否已经超过了最大缓冲周期时间(不判断音/视频、I/P帧)
  // 2. 当时间超过了阀值,查找最新的fKeyFrameStartPacketElementPointer
  // 3. 返回最新的fKeyFrameStartPacketElementPointer做为最新的BookMark

  SInt64 theCurrentTime = Core::Time::Milliseconds();
  SInt64 packetDelay = 0;
  SInt64 currentMaxPacketDelay = ReflectorStream::sRelocatePacketAgeMSec;

  ReflectorPacket *thePacket = (ReflectorPacket *) elem->GetEnclosingObject();
  Assert(thePacket);

  packetDelay = theCurrentTime - thePacket->fTimeArrived;

  if ((packetDelay > currentMaxPacketDelay) && IsKeyFrameFirstPacket(thePacket)) { // or IsFrameFirstPacket
    if (fKeyFrameStartPacketElementPointer) {
      ReflectorPacket *keyPacket = (ReflectorPacket *) (fKeyFrameStartPacketElementPointer->GetEnclosingObject());
      if (keyPacket->fTimeArrived > thePacket->fTimeArrived) {
        this->fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(true);
        return fKeyFrameStartPacketElementPointer;
      }
    }
  }

  return elem;
}

QueueElem *ReflectorSender:: GetNewestKeyFrameFirstPacket(QueueElem *currentElem, SInt64 offsetMsec) {
  //printf("[geyijun] GetNewestKeyFrameFirstPacket---------------->1\n");
  SInt64 theCurrentTime = Core::Time::Milliseconds();
  SInt64 packetDelay = 0;
  QueueElem *requestedPacket = NULL;
  QueueIter qIter(&fPacketQueue, currentElem);
  while (!qIter.IsDone()) { // start at oldest packet in q
    QueueElem *elem = qIter.GetCurrent();
    Assert(elem);
    qIter.Next();

    ReflectorPacket *thePacket = (ReflectorPacket *) elem->GetEnclosingObject();
    if (thePacket == NULL) {
      break;
    }
    if (IsKeyFrameFirstPacket(thePacket)) {
      requestedPacket = &thePacket->fQueueElem;
      //printf("[geyijun]Maybe,GetNewestKeyFrameFirstPacket --->[%#x]\n",requestedPacket);

      //
      packetDelay = theCurrentTime - thePacket->fTimeArrived;
      if (offsetMsec > ReflectorStream::sOverBufferInMsec)
        offsetMsec = ReflectorStream::sOverBufferInMsec;
      if (packetDelay <= (ReflectorStream::sOverBufferInMsec - offsetMsec)) {
        break;
      }
    }
  }
  if (requestedPacket == NULL) {
    printf("[geyijun]GetNewestKeyFrameFirstPacket --->[NotFound]\n");
  } else {
    printf("[geyijun]Final,GetNewestKeyFrameFirstPacket --->[%#x]\n", requestedPacket);
  }
  return requestedPacket;
}

char const* nal_unit_type_description_h264[32] = {
    "Unspecified", //0
    "Coded slice of a non-IDR picture", //1
    "Coded slice data partition A", //2
    "Coded slice data partition B", //3
    "Coded slice data partition C", //4
    "Coded slice of an IDR picture", //5
    "Supplemental enhancement information (SEI)", //6
    "Sequence parameter set", //7
    "Picture parameter set", //8
    "Access unit delimiter", //9
    "End of sequence", //10
    "End of stream", //11
    "Filler data", //12
    "Sequence parameter set extension", //13
    "Prefix NAL unit", //14
    "Subset sequence parameter set", //15
    "Reserved", //16
    "Reserved", //17
    "Reserved", //18
    "Coded slice of an auxiliary coded picture without partitioning", //19
    "Coded slice extension", //20
    "Reserved", //21
    "Reserved", //22
    "Reserved", //23
    "Unspecified", //24
    "Unspecified", //25
    "Unspecified", //26
    "Unspecified", //27
    "Unspecified", //28
    "Unspecified", //29
    "Unspecified", //30
    "Unspecified" //31
};


/**
 * 判断当前RTP包是否为H.264 I关键帧的第一个RTP包
 * 下面的写法可能不太严谨(仅针对H264 的情况，未考虑其他格式)
 */
bool ReflectorSender::IsKeyFrameFirstPacket(ReflectorPacket *thePacket) {
  Assert(thePacket);

  if ((thePacket->fPacketPtr.Ptr != NULL) && (thePacket->fPacketPtr.Len >= 20)) {
    auto *rtpHeader = reinterpret_cast<RTPFixedHeader*>(thePacket->fPacketPtr.Ptr);
    UInt32 rtpHeaderLen = sizeof(RTPFixedHeader) + rtpHeader->cc * sizeof(UInt32);
    auto *nalHeader = reinterpret_cast<NALUHeader*>(&thePacket->fPacketPtr.Ptr[rtpHeaderLen]);
    UInt8 naluType = 0;
    if (nalHeader->type >= 1 && nalHeader->type <= 23) { // 单一包
      naluType = nalHeader->type;
    } else if (nalHeader->type == 24) { // STAP-A
      // rtp header + STAP-A nal header + nalu size
      UInt32 naluOffset = rtpHeaderLen + sizeof(NALUHeader) + sizeof(UInt16);
      if (thePacket->fPacketPtr.Len > naluOffset) {
        auto *naluHeader = reinterpret_cast<NALUHeader*>(&thePacket->fPacketPtr.Ptr[naluOffset]);
        naluType = naluHeader->type;
      }
    } else if (nalHeader->type == 25) { // STAP-B
      // rtp header + STAP-B nal header + DON + nalu size
      UInt32 naluOffset = rtpHeaderLen  + sizeof(NALUHeader) + sizeof(UInt16) + sizeof(UInt16);
      if (thePacket->fPacketPtr.Len > naluOffset) {
        auto *naluHeader = reinterpret_cast<NALUHeader*>(&thePacket->fPacketPtr.Ptr[naluOffset]);
        naluType = naluHeader->type;
      }
    } else if (nalHeader->type == 26) { // MTAP16
      // rtp header + MTAP16 nal header + DONB + nalu size + nalu DOND + nalu TS offset(16 bits)
      UInt32 naluOffset = rtpHeaderLen + sizeof(NALUHeader) + sizeof(UInt16) + sizeof(UInt16) + sizeof(UInt8) + 2;
      if (thePacket->fPacketPtr.Len > naluOffset) {
        auto *naluHeader = reinterpret_cast<NALUHeader*>(&thePacket->fPacketPtr.Ptr[naluOffset]);
        naluType = naluHeader->type;
      }
    } else if (nalHeader->type == 27) { // MTAP24
      // rtp header + MTAP16 nal header + DONB + nalu size + nalu DOND + nalu TS offset(24 bits)
      UInt32 naluOffset = rtpHeaderLen + sizeof(NALUHeader) + sizeof(UInt16) + sizeof(UInt16) + sizeof(UInt8) + 3;
      if (thePacket->fPacketPtr.Len > naluOffset) {
        auto *naluHeader = reinterpret_cast<NALUHeader*>(&thePacket->fPacketPtr.Ptr[naluOffset]);
        naluType = naluHeader->type;
      }
    } else if (nalHeader->type == 28 || nalHeader->type == 29) { // FU-A/B
      // rtp header + FU indicator
      UInt32 fuOffset = rtpHeaderLen + sizeof(FUIndicator);
      if (thePacket->fPacketPtr.Len > fuOffset) {
        auto *fuHeader = reinterpret_cast<FUHeader*>(&thePacket->fPacketPtr.Ptr[fuOffset]);
        if (fuHeader->s) { // 起始包
          naluType = fuHeader->type;
        }
      }
    }

    if ((naluType == 5) || (naluType == 7) || (naluType == 8)) { // IDR/SPS/PPS
      return true;
    }
  }
  return false;
}

bool ReflectorSender::IsFrameFirstPacket(ReflectorPacket *thePacket) {
  //printf("[geyijun] IsKeyFrameFirstPacket--->RtpSeq[%d]\n",thePacket->GetPacketRTPSeqNum());
  Assert(thePacket);
  if ((thePacket->fPacketPtr.Ptr != NULL)
      && (thePacket->fPacketPtr.Len >= 20)) {
    UInt8 csrc_count = thePacket->fPacketPtr.Ptr[0] & 0x0f;
    UInt32 rtp_head_size = /*sizeof(struct RTPHeader)*/
        12 + csrc_count * sizeof(UInt32);
    UInt8 nal_unit_type = thePacket->fPacketPtr.Ptr[rtp_head_size + 0] & 0x1F;
    if ((nal_unit_type >= 1) && (nal_unit_type <= 23)) {    //单一包
      return true;
    } else if (nal_unit_type == 24) { //STAP-A
      return true;
    } else if (nal_unit_type == 25) { //STAP-B
      return true;
    } else if (nal_unit_type == 26) { //MTAP16
      return true;
    } else if (nal_unit_type == 27) { //MTAP24
      return true;
    } else if ((nal_unit_type == 28) || (nal_unit_type == 29)) { //FU-A/B
      if (thePacket->fPacketPtr.Len > rtp_head_size + 1) {
        UInt8 startBit = thePacket->fPacketPtr.Ptr[rtp_head_size + 1] & 0x80;
        if (startBit) {
          return true;
        }
      }
    }
  }
  return false;
}

bool ReflectorSender::IsFrameLastPacket(ReflectorPacket *thePacket) {
  //printf("[geyijun] IsFrameLastPacket--->RtpSeq[%d]\n",thePacket->GetPacketRTPSeqNum());
  Assert(thePacket);
  if ((thePacket->fPacketPtr.Ptr != NULL) && (thePacket->fPacketPtr.Len >= 20)) {
    UInt8 markBit = thePacket->fPacketPtr.Ptr[1] & 0x80;
    if (markBit) {
      //printf("[geyijun] IsFrameLastPacket --->OK\n");
      return true;
    }
  }
  return false;
}

void ReflectorSocketPool::SetUDPSocketOptions(Net::UDPSocketPair *inPair) {
  // Fix add ReuseAddr for compatibility with MPEG4IP broadcaster which likes to use the same
  //sockets.

  //Make sure this works with PlaylistBroadcaster
  //inPair->GetSocketA()->ReuseAddr();
  //inPair->GetSocketA()->ReuseAddr();

}

Net::UDPSocketPair *ReflectorSocketPool::ConstructUDPSocketPair() {
  return new Net::UDPSocketPair(new ReflectorSocket(), new ReflectorSocket());
}

void ReflectorSocketPool::DestructUDPSocket(ReflectorSocket *socket) {
  if (socket) { // allocated
    //if (socket->GetLocalPort() > 0) // bound and active
    //{   //The socket's run function may be executing RIGHT NOW! So we can't
    //just delete the thing, we need to send the sockets kill events.
    //s_printf("ReflectorSocketPool::DestructUDPSocketPair Signal kKillEvent socket=%p\n",socket);
    socket->Signal(Thread::Task::kKillEvent);
    //}
    //else // not bound ok to delete
    //{   //s_printf("ReflectorSocketPool::DestructUDPSocketPair delete socket=%p\n",socket);
    //    delete socket;
    //}
  }
}

void ReflectorSocketPool::DestructUDPSocketPair(Net::UDPSocketPair *inPair) {
  //s_printf("ReflectorSocketPool::DestructUDPSocketPair inPair=%p socketA=%p\n", inPair,(ReflectorSocket*)inPair->GetSocketA());
  this->DestructUDPSocket((ReflectorSocket *) inPair->GetSocketA());

  //s_printf("ReflectorSocketPool::DestructUDPSocketPair inPair=%p socketB=%p\n", inPair,(ReflectorSocket*)inPair->GetSocketB());
  this->DestructUDPSocket((ReflectorSocket *) inPair->GetSocketB());

  delete inPair;
}

ReflectorSocket::ReflectorSocket()
    : IdleTask(),
      UDPSocket(NULL, Net::Socket::kNonBlockingSocketType | Net::UDPSocket::kWantsDemuxer),
      fBroadcasterClientSession(NULL),
      fLastBroadcasterTimeOutRefresh(0),
      fSleepTime(0),
      fValidSSRC(0),
      fLastValidSSRCTime(0),
      fFilterSSRCs(true),
      fTimeoutSecs(30),
      fHasReceiveTime(false),
      fFirstReceiveTime(0),
      fFirstArrivalTime(0),
      fCurrentSSRC(0) {
  // construct all the preallocated packets
  this->SetTaskName("ReflectorSocket");
  this->SetTask(this);

  for (UInt32 numPackets = 0; numPackets < kNumPreallocatedPackets; numPackets++) {
    // If the local port # of this socket is odd, then all the packets
    // used for this socket are rtcp packets.
    ReflectorPacket *packet = new ReflectorPacket();
    fFreeQueue.EnQueue(&packet->fQueueElem);  // put this packet onto the free queue
  }
}

ReflectorSocket::~ReflectorSocket() {
  //printf("ReflectorSocket::~ReflectorSocket\n");
  while (fFreeQueue.GetLength() > 0) {
    auto *packet = (ReflectorPacket *) fFreeQueue.DeQueue()->GetEnclosingObject();
    delete packet;
  }
}

void ReflectorSocket::AddSender(ReflectorSender *inSender) {
  Core::MutexLocker locker(this->GetDemuxer()->GetMutex());
  QTSS_Error err = this->GetDemuxer()->RegisterTask(inSender->fStream->fStreamInfo.fSrcIPAddr, 0, inSender);
  Assert(err == QTSS_NoErr);
  fSenderQueue.EnQueue(&inSender->fSocketQueueElem);
}

void ReflectorSocket::RemoveSender(ReflectorSender *inSender) {
  Core::MutexLocker locker(this->GetDemuxer()->GetMutex());
  fSenderQueue.Remove(&inSender->fSocketQueueElem);
  QTSS_Error err = this->GetDemuxer()->UnregisterTask(inSender->fStream->fStreamInfo.fSrcIPAddr, 0, inSender);
  Assert(err == QTSS_NoErr);
}

/*
 * ReflectorSocket::Run() -> ReflectorSender::ReflectPackets() -> ReflectorSender::SendPacketsToOutput() -> ReflectorOutput::WritePacket()
 * 当 ReflectorSocket 接受到推流上来的数据后，会启动 Task。
 * 在 ReflectorSocket::Run() 中，遍历 fSenderQueue，并调用每个 Sender 的 ReflectPackets()
 * 在 ReflectorSender::ReflectPackets() 中，遍历与 fStream 相关联的 ReflectorOutput，并对 Output 调用 SendPacketsToOutput()
 * 在 ReflectorSender::SendPacketsToOutput() 中，遍历 fPacketQueue，并对每个 Packet 调用 Output 的 WritePacket()
 * 在 RTPSessionOutput::WritePacket() 中，通过 StreamCookie 在与 Output 对应的 ClientSession 中找到正确的 RTPStream，
 *     调用 QTSS_Write() 将 Packet 通过 RTPStream 发送出去
 */
SInt64 ReflectorSocket::Run() {
  // We want to make sure we can't get idle events WHILE we are inside
  // this function. That will cause us to run the queues unnecessarily
  // and just get all confused.
  this->CancelTimeout();

  EventFlags theEvents = this->GetEvents();

  //if we have been told to delete ourselves, do so.
  if (theEvents & kKillEvent)
    return -1;

  Core::MutexLocker locker(this->GetDemuxer()->GetMutex());
  SInt64 theMilliseconds = Core::Time::Milliseconds();

  // Only check for data on the socket if we've actually been notified to that effect
  if (theEvents & kReadEvent)
    this->GetIncomingData(theMilliseconds);

#if DEBUG
  //make sure that we haven't gotten here prematurely! This wouldn't mess
  //anything up, but it would waste CPU.
  if (theEvents & Task::kIdleEvent) {
      SInt32 temp = (SInt32)(fSleepTime - theMilliseconds);
      char tempBuf[20];
      s_sprintf(tempBuf, "%" _S32BITARG_ "", temp);
      WarnV(fSleepTime <= theMilliseconds, tempBuf);
  }
#endif

  fSleepTime = 0;
  // Now that we've gotten all available packets, have the streams reflect
  for (QueueIter iter2(&fSenderQueue); !iter2.IsDone(); iter2.Next()) {
    ReflectorSender *theSender2 = (ReflectorSender *) iter2.GetCurrent()->GetEnclosingObject();
    // 根据 fNextTimeToRun 和当前时间判断是否马上进行 Reflect.
    if (theSender2 != NULL && theSender2->ShouldReflectNow(theMilliseconds, &fSleepTime))
      theSender2->ReflectPackets(&fSleepTime, &fFreeQueue);
  }

#if DEBUG
  theMilliseconds = OS::Milliseconds();
#endif

  // For smoothing purposes, the streams can mark when they want to wakeup.
  // fSleepTime 实际上是 Sender 的 fNextTimeToRun,利用 IdleTimerThread 实现下一次的继续运行。
  if (fSleepTime > 0)
    this->SetIdleTimer(fSleepTime);
#if DEBUG
  //The debugging check above expects real time.
  fSleepTime += theMilliseconds;
#endif

  return 0;
}

/**
 * assume the first SSRC we see is valid and all others are to be ignored.
 */
bool ReflectorSocket::FilterInvalidSSRCs(ReflectorPacket *thePacket) {
  bool invalid = false;

  do {
    if (thePacket->fPacketPtr.Len <= 0) break;

    SInt64 currentTime = Core::Time::Milliseconds() / 1000;

    if (0 == fValidSSRC) {
      fValidSSRC = thePacket->GetSSRC(); // SSRC of 0 is allowed
      fLastValidSSRCTime = currentTime;
      //s_printf("socket=%"   _U32BITARG_   " FIRST PACKET fValidSSRC=%"   _U32BITARG_   " \n", (UInt32) this,fValidSSRC);
      break;
    }

    UInt32 packetSSRC = thePacket->GetSSRC();
    if (packetSSRC != 0) {
      if (packetSSRC == fValidSSRC) {
        fLastValidSSRCTime = currentTime;
        //s_printf("socket=%"   _U32BITARG_   " good packet\n", (UInt32) this );
        break;
      }

      //s_printf("socket=%"   _U32BITARG_   " bad packet packetSSRC= %"   _U32BITARG_   " fValidSSRC=%"   _U32BITARG_   " \n", (UInt32) this,packetSSRC,fValidSSRC);
      thePacket->fPacketPtr.Len = 0; // ignore this packet wrong SSRC
      invalid = true;
    }

    // this executes whenever an invalid SSRC is found -- maybe the original stream ended and a new one is now active
    if ((fLastValidSSRCTime + fTimeoutSecs) < currentTime) { // fValidSSRC timed out --no packets with this SSRC seen for awhile
      fValidSSRC = 0; // reset the valid SSRC with the next packet's SSRC
      //s_printf("RESET fValidSSRC\n");
    }
  } while (false);

  return invalid;
}

void ReflectorSocket::BufferKeyFrame(ReflectorSender *theSender, ReflectorPacket *thePacket) {
  //
  // 对H264视频RTP包进行关键帧过滤，保存最新关键帧首个RTP包指针

  // 1. 判断是否为视频 H.264 RTP
  SourceInfo::StreamInfo *streamInfo = theSender->fStream->GetStreamInfo();
  if ((streamInfo->fPayloadType == qtssVideoPayloadType) &&
      (streamInfo->fPayloadName.Equal("H264/90000"))) { // h.264 payload 固定为 H264/90000

    // 2. 在这里判断上面插入的thePacket是否为关键帧起始RTP包，如果是，则记录thePacket->fQueueElem
    if (theSender->IsKeyFrameFirstPacket(thePacket)) {

      // 3. 取消原来的fKeyFrameStartPacketElementPointer
      if (theSender->fKeyFrameStartPacketElementPointer) {
        auto *oldKeyFramePacket = (ReflectorPacket *)
            theSender->fKeyFrameStartPacketElementPointer->GetEnclosingObject();
        oldKeyFramePacket->fNeededByOutput = false;
      }

      // 4. 设置最新的fKeyFrameStartPacketElementPointer
      thePacket->fNeededByOutput = true; // 锁定关键帧不会被Remove
      theSender->fKeyFrameStartPacketElementPointer = &thePacket->fQueueElem; // 更新最新的关键帧开始包

      // 5. 设置ReflectorSession标志位，Notify有新视频关键帧，提醒音频队列更新
      theSender->fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(true);
    }
  }

  //
  // 如果视频关键帧有更新，音频也根据视频的更新立即进行更新

  // 1. 判断是否为音频RTP且有视频关键帧更新Notify
  else if ((streamInfo->fPayloadType == qtssAudioPayloadType) &&
      (theSender->fStream->GetMyReflectorSession()->HasVideoKeyFrameUpdate())) {

    // 2. 取消原来音频的fKeyFrameStartPacketElementPointer
    if (theSender->fKeyFrameStartPacketElementPointer) {
      auto *oldKeyFramePacket = (ReflectorPacket *)
          theSender->fKeyFrameStartPacketElementPointer->GetEnclosingObject();
      oldKeyFramePacket->fNeededByOutput = false;
    }

    // 4. 设置最新的音频fKeyFrameStaretElementPointer
    thePacket->fNeededByOutput = true; // 锁定关键帧不会被Remove
    theSender->fKeyFrameStartPacketElementPointer = &thePacket->fQueueElem; // 更新最新的关键帧开始包

    // 5. 设置ReflectorSession标志位，Notify有新视频关键帧，提醒音频队列更新
    theSender->fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(false);
  }
}

/**
 * 将单个 ReflectorPacket 分发到适当的 ReflectorSender 中
 *
 * @note RTP over RTSP(TCP) 的合包请求会先被分拆，函数调用时传递进来的 thePacket 总是单个 RTP 包，且该包必有数据
 *
 * @see RTSPRequestStream::ReadRequest
 *
 */
void ReflectorSocket::
ProcessPacket(const SInt64 &inMilliseconds, ReflectorPacket *thePacket, UInt32 theRemoteAddr, UInt16 theRemotePort) {

  if (fBroadcasterClientSession != nullptr) { // always refresh timeout even if we are filtering.
    if ((inMilliseconds - fLastBroadcasterTimeOutRefresh) > kRefreshBroadcastSessionIntervalMilliSecs) {
      QTSS_RefreshTimeOut(fBroadcasterClientSession);
      fLastBroadcasterTimeOutRefresh = inMilliseconds;
    }
  }

  // Only reflect one SSRC stream at a time.
  // Pass the packet and whether it is an RTCP or RTP packet based on the port number.
  if (fFilterSSRCs) {
    // thePacket->fPacketPtr.Len is set to 0 for invalid SSRCs, and pass
    if (this->FilterInvalidSSRCs(thePacket)) return;
  }

  if (thePacket->IsRTCP()) {
    // if this is a new RTCP packet, check to see if it is a sender report.
    // We should only reflect sender reports. Because RTCP packets can't have both
    // an SR & an RR, and because the SR & the RR must be the first packet in a
    // compound RTCP packet, all we have to do to determine this is look at the
    // packet type of the first packet in the compound packet.
    RTCPPacket theRTCPPacket;
    if ((!theRTCPPacket.ParsePacket((UInt8 *) thePacket->fPacketPtr.Ptr, thePacket->fPacketPtr.Len)) ||
        (theRTCPPacket.GetPacketType() != RTCPSRPacket::kSRPacketType)) {
      // pretend as if we never got this packet
      fFreeQueue.EnQueue(&thePacket->fQueueElem);
//      this->RequestEvent(EV_RM); // 不再接收 RTCP 包
      return;
    }
  }

  // Find the appropriate ReflectorSender for this packet.
  // 在 BindSockets 函数里,已经对 Socket A/B 调用 AddSender 注册了 fRTPSender、fRTCPSender
  auto *theSender = (ReflectorSender *) this->GetDemuxer()->GetTask(theRemoteAddr, 0);
  // If there is a generic sender for this socket, use it.
  if (theSender == nullptr)
    theSender = (ReflectorSender *) this->GetDemuxer()->GetTask(0, 0);

  if (theSender == nullptr) {
    //UInt16* theSeqNumberP = (UInt16*)thePacket->fPacketPtr.Ptr;
    //s_printf("ReflectorSocket::ProcessPacket no sender found for packet! sequence number=%d\n",ntohs(theSeqNumberP[1]));
    fFreeQueue.EnQueue(&thePacket->fQueueElem); // don't process the packet
    return;
  }

  Assert(theSender != NULL); // at this point we have a sender


  // Check to see if we need to set the remote RTCP address for this stream.
  // This will be necessary if the source is unicast.
#ifdef NAT_WORKAROUND
  if ((theRemoteAddr != 0) && ((theSender->fStream->fDestRTCPAddr == 0) || (thePacket->IsRTCP()))) {
    Assert(!Net::SocketUtils::IsMulticastIPAddr(theSender->fStream->fStreamInfo.fDestIPAddr));
    Assert(theRemotePort != 0);
    theSender->fStream->fDestRTCPAddr = theRemoteAddr;
    theSender->fStream->fDestRTCPPort = theRemotePort;

    // RTCPs are always on odd ports, so check to see if this port is an RTP port,
    // and if so, just add 1.
    if (!thePacket->IsRTCP() && !(theRemotePort & 1U))
      theSender->fStream->fDestRTCPPort++;
  }
#else
  if ((theRemoteAddr != 0) && (theSender->fStream->fDestRTCPAddr == 0)) {
      // If the source is multicast, this shouldn't be necessary
      Assert(!SocketUtils::IsMulticastIPAddr(theSender->fStream->fStreamInfo.fDestIPAddr));
      Assert(theRemotePort != 0);
      theSender->fStream->fDestRTCPAddr = theRemoteAddr;
      theSender->fStream->fDestRTCPPort = theRemotePort;

      // RTCPs are always on odd ports, so check to see if this port is an
      // RTP port, and if so, just add 1.
      if (!(theRemotePort & 1))
          theSender->fStream->fDestRTCPPort++;
  }
#endif //NAT_WORKAROUND

  thePacket->fStreamCountID = ++(theSender->fStream->fPacketCount);
  thePacket->fBucketsSeenThisPacket = 0;
  thePacket->fTimeArrived = inMilliseconds;

  // Push to sender's packet queue
  theSender->fPacketQueue.EnQueue(&thePacket->fQueueElem);
  if (theSender->fFirstNewPacketInQueue == nullptr)
    theSender->fFirstNewPacketInQueue = &thePacket->fQueueElem;
  theSender->fHasNewPackets = true;

  if (!thePacket->IsRTCP()) {
    // 关键帧缓冲 for h.264
    BufferKeyFrame(theSender, thePacket);

    // don't check for duplicate packets, they may be needed to keep in sync.
    // Because this is an RTP packet, make sure to atomic add this because
    // multiple sockets can be adding to this variable simultaneously
    theSender->fStream->fBytesSentInThisInterval.fetch_add(thePacket->fPacketPtr.Len);

    //printf("ReflectorSocket::ProcessPacket received RTP id=%qu\n", thePacket->fStreamCountID);
    theSender->fStream->SetHasFirstRTP(true);
  } else {
    //printf("ReflectorSocket::ProcessPacket received RTCP id=%qu\n", thePacket->fStreamCountID);
    theSender->fStream->SetHasFirstRTCP(true);
    theSender->fStream->SetFirst_RTCP_RTP_Time(thePacket->GetPacketRTPTime());
    theSender->fStream->SetFirst_RTCP_Arrival_Time(thePacket->fTimeArrived);
  }

  if (ReflectorStream::sUsePacketReceiveTime && thePacket->fPacketPtr.Len > 12) { // default is false
    UInt32 offset = thePacket->fPacketPtr.Len;
    char *theTag = thePacket->fPacketPtr.Ptr + offset - 12;
    auto *theValue = (UInt64 *) (thePacket->fPacketPtr.Ptr + offset - 8);

    if (0 == ::strncmp(theTag, "aktt", 4)) {
      UInt64 theReceiveTime = Utils::NetworkToHostUInt64(*theValue);
      UInt32 theSSRC = thePacket->GetSSRC(); // use to check if broadcast has restarted so we can reset

      if (!this->fHasReceiveTime || (this->fCurrentSSRC != theSSRC)) {
        this->fCurrentSSRC = theSSRC;
        this->fFirstArrivalTime = thePacket->fTimeArrived;
        this->fFirstReceiveTime = theReceiveTime;
        this->fHasReceiveTime = true;
      }

      SInt64 packetOffsetFromStart = theReceiveTime - this->fFirstReceiveTime; // packets arrive at time 0 and fill forward into the future
      thePacket->fTimeArrived = this->fFirstArrivalTime + packetOffsetFromStart; // offset starts negative by over buffer amount
      thePacket->fPacketPtr.Len -= 12;

      SInt64 arrivalTimeOffset = thePacket->fTimeArrived - inMilliseconds;
      if (arrivalTimeOffset > ReflectorStream::sMaxFuturePacketMSec) // way out in the future.
        thePacket->fTimeArrived = inMilliseconds + ReflectorStream::sMaxFuturePacketMSec; //keep it but only for sMaxFuturePacketMSec =  (sMaxPacketAgeMSec <-- current --> sMaxFuturePacketMSec)

      // if it was in the past we leave it alone because it will be deleted after processing.

      //printf("ReflectorSocket::ProcessPacket packetOffsetFromStart=%f\n", (Float32) packetOffsetFromStart / 1000);
    }
  }

  //printf("ReflectorSocket::GetIncomingData has packet from time=%qd src addr=%"   _U32BITARG_   " src port=%u packetlen=%"   _U32BITARG_   "\n",inMilliseconds, theRemoteAddr,theRemotePort,thePacket->fPacketPtr.Len);
  if (0) {//turn on / off buffer size checking --  pref can go here if we find we need to adjust this
    const UInt32 maxQSize = 4000;
    if (theSender->fPacketQueue.GetLength() > maxQSize) { //don't grow memory too big
      char outMessage[256];
      sprintf(outMessage, "Packet Queue for port=%d qsize = %" _S32BITARG_ " hit max qSize=%" _U32BITARG_ "",
              theRemotePort, theSender->fPacketQueue.GetLength(), maxQSize);
      WarnV(false, outMessage);
    }
  }
}

void ReflectorSocket::GetIncomingData(const SInt64 &inMilliseconds) {
  Core::MutexLocker locker(this->GetDemuxer()->GetMutex());
  UInt32 theRemoteAddr = 0;
  UInt16 theRemotePort = 0;

  // get all the outstanding packets for this socket
  for (;;) {
    // get a packet off the free queue.
    ReflectorPacket *thePacket = this->GetPacket();

    // 调用 ::recvfrom 从 socket 里读取数据到 thePacket 的缓冲区
    thePacket->fPacketPtr.Len = 0;
    (void) this->RecvFrom(&theRemoteAddr, &theRemotePort, thePacket->fPacketPtr.Ptr,
                          ReflectorPacket::kMaxReflectorPacketSize, &thePacket->fPacketPtr.Len);

    if (thePacket->fPacketPtr.Len == 0) {
      // put the packet back on the free queue, because we didn't actually get any data here.
      fFreeQueue.EnQueue(&thePacket->fQueueElem);
      //s_printf("ReflectorSocket::GetIncomingData no more packets on this socket!\n");
      break;  // no more packets on this socket!
    }

    // if the port number of this socket is odd, this packet is an RTCP packet.
    thePacket->fIsRTCP = static_cast<bool>(this->GetLocalPort() & 1U);

    // 获取 Socket 对应的 Sender,对 Sender、thePacket 进行一系列设置,最终将 thePacket
    // 挂入 Sender 的 fPacketQueue
    this->ProcessPacket(inMilliseconds, thePacket, theRemoteAddr, theRemotePort);

    //s_printf("ReflectorSocket::GetIncomingData \n");
  }

//  this->RequestEvent(EV_RE); // re watch EV_RE
}

/**
 * pop from fFreeQueue or new one.
 */
ReflectorPacket *ReflectorSocket::GetPacket() {
  Core::MutexLocker locker(this->GetDemuxer()->GetMutex());
  if (fFreeQueue.GetLength() == 0)
    return new ReflectorPacket();
  else
    return (ReflectorPacket *) fFreeQueue.DeQueue()->GetEnclosingObject();
}
