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
    File:       ReflectorOutput.h

    Contains:   VERY simple abstract base class that defines one virtual method, WritePacket.
                This is extremely useful to the reflector, which, using one of these objects,
                can transparently reflect a packet, not being aware of how it will actually be
                written to the network
                    


*/

#ifndef __REFLECTOR_OUTPUT_H__
#define __REFLECTOR_OUTPUT_H__

#include <CF/Core/Mutex.h>
#include <CF/StrPtrLen.h>
#include <CF/Queue.h>

#include "QTSS.h"

class ReflectorOutput {
 public:

  ReflectorOutput()
      : fBookmarkedPacketsElemsArray(nullptr), fNumBookmarks(0), fAvailPosition(0),
        fLastIntervalMilliSec(5), fLastPacketTransmitTime(0) {}

  virtual ~ReflectorOutput() {
    if (fBookmarkedPacketsElemsArray) {
      ::memset(fBookmarkedPacketsElemsArray, 0, sizeof(CF::QueueElem *) * fNumBookmarks);
      delete[] fBookmarkedPacketsElemsArray;
    }
  }

  // an array of packet elements ( from fPacketQueue in ReflectorSender )
  // possibly one for each ReflectorSender that sends data to this ReflectorOutput
  CF::QueueElem **fBookmarkedPacketsElemsArray;
  UInt32 fNumBookmarks;
  SInt32 fAvailPosition;
  QTSS_TimeVal fLastIntervalMilliSec;
  QTSS_TimeVal fLastPacketTransmitTime;
  CF::Core::Mutex fMutex;

  //add by fantasy
 private:
  UInt64 fU64Seq;
  bool fNewOutput;

 public:
  UInt64 outPutSeq() {
    return fU64Seq;
  }

  bool addSeq() {
    fU64Seq++;
    if (fU64Seq == 0xffffffffffffffff) {
      fU64Seq = 0;
    }
    return true;
  }

  bool getNewFlag() {
    return fNewOutput;
  }

  void setNewFlag(bool flag) {
    fNewOutput = flag;
  }
  //end add


  inline CF::QueueElem *GetBookMarkedPacket(CF::Queue *thePacketQueue);

  inline bool SetBookMarkPacket(CF::QueueElem *thePacketElemPtr);

  /**
   * 将 Packet 通过 inStreamCookie 标记的 RTPStream 发送出去
   *
   * @param inPacket  the packet contents
   * @param inStreamCookie  the cookie of the stream to which it will be written
   * @param inFlags  the QTSS API write flags (qtssWriteFlagsIsRTP or qtssWriteFlagsIsRTCP)
   * @param packetLatenessInMSec  how many MSec's late this packet is in being delivered (<0 if its early)
   *
   * @return QTSS_WouldBlock  timeToSendThisPacketAgain will be set to # of msec in which the packet can be sent
   * @return -1  unknown
   */
  virtual QTSS_Error WritePacket(CF::StrPtrLen *inPacket, void *inStreamCookie, UInt32 inFlags,
                                 SInt64 packetLatenessInMSec, SInt64 *timeToSendThisPacketAgain,
                                 UInt64 *packetIDPtr, SInt64 *arrivalTimeMSec, bool firstPacket) = 0;

  virtual void TearDown() = 0;

  virtual bool IsUDP() = 0;

  virtual bool IsPlaying() = 0;

  enum {
    kWaitMilliSec = 5, kMaxWaitMilliSec = 1000
  };

 protected:

  void InitializeBookmarks(UInt32 numStreams) {
    // need 2 bookmarks for each stream ( include RTCPs )
    UInt32 numBookmarks = numStreams * 2;

    fBookmarkedPacketsElemsArray = new CF::QueueElem *[numBookmarks];
    ::memset(fBookmarkedPacketsElemsArray, 0, sizeof(CF::QueueElem *) * (numBookmarks));

    //DT("fBookmarkedPacketsElemsArray[-1] %p= %p", &fBookmarkedPacketsElemsArray[-1], fBookmarkedPacketsElemsArray[-1]);
    fNumBookmarks = numBookmarks;
  }

};

bool ReflectorOutput::SetBookMarkPacket(CF::QueueElem *thePacketElemPtr) {
  if (fAvailPosition != -1 && thePacketElemPtr) {
    fBookmarkedPacketsElemsArray[fAvailPosition] = thePacketElemPtr;

    // 定位到另一个的可用位置
    for (UInt32 i = 0; i < fNumBookmarks; i++) {
      if (fBookmarkedPacketsElemsArray[i] == nullptr) {
        fAvailPosition = i;
        return true;
      }
    }
  }

  return false;
}

CF::QueueElem *ReflectorOutput::GetBookMarkedPacket(CF::Queue *thePacketQueue) {
  Assert(thePacketQueue != nullptr);

  CF::QueueElem *packetElem = nullptr;

  fAvailPosition = -1;

  // see if we've bookmarked a held packet for this Sender in this Output
  for (UInt32 curBookmark = 0; curBookmark < fNumBookmarks; curBookmark++) {
    CF::QueueElem *bookmarkedElem = fBookmarkedPacketsElemsArray[curBookmark];
    if (bookmarkedElem) {  // there may be holes in this array
      if (bookmarkedElem->IsMember(*thePacketQueue)) {
        // this packet was previously bookmarked for this specific queue
        // remove if from the bookmark list and use it
        // to jump ahead into the Sender's over all packet queue
        fBookmarkedPacketsElemsArray[curBookmark] = nullptr;
        fAvailPosition = curBookmark;
        packetElem = bookmarkedElem;
        break;
      }
    } else {
      fAvailPosition = curBookmark;
    }
  }

  return packetElem;
}

#endif //__REFLECTOR_OUTPUT_H__
