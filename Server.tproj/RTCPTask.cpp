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
 * @file RTCPTask.cpp
 *
 * Contains:   Implementation of class defined in RTCPTask.h
 */

#include <CF/Net/Socket/UDPSocketPool.h>

#include "RTCPTask.h"
#include "QTSServerInterface.h"
#include "RTPStream.h"

SInt64 RTCPTask::Run() {
  // 在运行StartTasks创建RTCPTask类后,该Run函数就会被调度运行。
  // 后续当监听到用来完成RTCP Data接收的Udp socket端口有数据时,
  // EventContext::ProcessEvent调用Signal(Task::kReadEvent),该函数会被运行。

  const UInt32 kMaxRTCPPacketSize = 2048;
  char thePacketBuffer[kMaxRTCPPacketSize];
  StrPtrLen thePacket(thePacketBuffer, 0);
  QTSServerInterface *theServer = QTSServerInterface::GetServer();

  // This task goes through all the UDPSockets in the RTPSocketPool, checking to see
  // if they have data. If they do, it demuxes the packets and sends the packet onto
  // the proper RTP session.
  EventFlags events = this->GetEvents(); // get and clear events

  if ((events & kReadEvent) || (events & kIdleEvent)) {
    //Must be done atomically write the socket pool.

    // 遍历SocketPool中的socket对中的每一个socket,如果这个socket有UDPDemuxer(在
    // RTPSocketPool::ConstructUDPSocketPair函数里,用来接收RTCP数据的Socket B
    // 就带有Demuxer),则将端口中接收到的数据发给UDPDemuxer对应的RTPStream,
    // 并调用RTPStream的ProcessIncomingRTCPPacket函数。
    Core::MutexLocker locker(theServer->GetSocketPool()->GetMutex());
    for (QueueIter iter(theServer->GetSocketPool()->GetSocketQueue());
         !iter.IsDone(); iter.Next()) {
      UInt32 theRemoteAddr = 0;
      UInt16 theRemotePort = 0;

      auto *thePair = (Net::UDPSocketPair *) iter.GetCurrent()->GetEnclosingObject();
      Assert(thePair != NULL);

      for (UInt32 x = 0; x < 2; x++) {
        Net::UDPSocket *theSocket = NULL;
        if (x == 0)
          theSocket = thePair->GetSocketA();
        else
          theSocket = thePair->GetSocketB();

        Net::UDPDemuxer *theDemuxer = theSocket->GetDemuxer();
        if (theDemuxer == NULL)
          continue;
        else {
          theDemuxer->GetMutex()->Lock();
          while (true) //get all the outstanding packets for this socket
          {
            thePacket.Len = 0;
            theSocket->RecvFrom(&theRemoteAddr, &theRemotePort, thePacket.Ptr,
                                kMaxRTCPPacketSize, &thePacket.Len);
            if (thePacket.Len == 0) {
              theSocket->RequestEvent(EV_RE);
              break;//no more packets on this socket!
            }

            //if this socket has a demuxer, find the target RTPStream
            if (theDemuxer != NULL) {
              // 在RTPStream的Setup函数里,曾经调用进行注册:
              // fSockets->GetSocketB()->GetDemuxer()->RegisterTask(fRemoteAddr, fRemoteRTCPPort, this);
              RTPStream *theStream = (RTPStream *) theDemuxer->GetTask(
                  theRemoteAddr,
                  theRemotePort);
              if (theStream != NULL)
                theStream->ProcessIncomingRTCPPacket(&thePacket);
            }
          }
          theDemuxer->GetMutex()->Unlock();
        }
      }
    }
  }

  return 0; /* Fix for 4004432 */
  /*
  SInt64 result = 0;
  if (theServer->GetNumRTPSessions() > 0)
      result =  theServer->GetPrefs()->GetRTCPPollIntervalInMsec();

 return result;
 */
}
