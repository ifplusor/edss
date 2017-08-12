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
    File:       TCPListenerSocket.cpp

    Contains:   implements TCPListenerSocket class


*/

#ifndef __Win32__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

#ifndef __Win32__

#include "QTSSModuleUtils.h"

#endif

#endif

#include "TCPListenerSocket.h"

OS_Error TCPListenerSocket::listen(UInt32 queueLength) {
  if (fFileDesc == EventContext::kInvalidFileDesc)
    return EBADF;

  int err = ::listen(fFileDesc, queueLength);
  if (err != 0)
    return (OS_Error) OSThread::GetErrno();
  return OS_NoErr;
}

OS_Error TCPListenerSocket::Initialize(UInt32 addr, UInt16 port) {
  // 创建打开流套接字(SOCK_STREAM)端口,并绑定 IP 地址、端口,执行 listen 操作。
  // 注意在这个函数里调用了 SetSocketRcvBufSize 成员函数,以设置这个 socket 的接收缓冲区大小为
  // 96K。而内核规定该值最大为 sysctl_rmem_max,可以通过/proc/sys/net/core/rmem_default
  // rmem_max 来了解缺省值和最大值。(注意 proc 下的这两个值也是可写的,可以通过调整这些值来
  // 达到优化 TCP/IP 的目的。

  OS_Error err = this->TCPSocket::Open();
  if (0 == err)
    do {
      // set SO_REUSEADDR socket option before calling bind.
#ifndef __Win32__
      // this causes problems on NT (multiple processes can bind simultaneously),
      // so don't do it on NT.
      this->ReuseAddr();
#endif
      err = this->Bind(addr, port);
      if (err != 0) break; // don't assert this is just a port already in use.

      //
      // Unfortunately we need to advertise a big buffer because our TCP sockets
      // can be used for incoming broadcast data. This could force the server
      // to run out of memory faster if it gets bogged down, but it is unavoidable.
      this->SetSocketRcvBufSize(512 * 1024);
      err = this->listen(kListenQueueLength);
      AssertV(err == 0, OSThread::GetErrno());
      if (err != 0) break;

    } while (false);

  return err;
}

void TCPListenerSocket::ProcessEvent(int /*eventBits*/) {
  // 在 fListeners 申请监听流套接字端口后,一旦 socket 端口有数据,该函数会被调用。
  // 这个函数的流程是这样的:
  // 首先通过 accept 获得客户端的地址以及服务器端新创建的 socket 端口。
  // 通过 GetSessionTask 创建一个 RTSPSession 的类对象<每个 RTSPSession 对象对应一个 RTSP 连接>,
  // 并将新创建的 socket 端口描述符、sockaddr 信息保存到 RTSPSession 的 TCPSocket 类型成员
  // fSocket。同时调用 fSocket::SetTask 和 RequestEvent(EV_RE),这样 EventThread 在监听到
  // 这个 socket 端口有数据时,会调用 EventContext::processEvent 函数,在这个函数里会调用
  // fTask->Signal(Task::kReadEvent),最终 TaskThread 会调用 RTSPSession::Run 函数。
  // 而 TCPListenerSocket 自己的 socket 端口会继续被申请监听。

  //we are executing on the same thread as every other
  //socket, so whatever you do here has to be fast.

  struct sockaddr_in addr;
#if __Win32__ || __osf__ || __sgi__ || __hpux__
  int size = sizeof(addr);
#else
  socklen_t size = sizeof(addr);
#endif
  Task *theTask = nullptr;
  TCPSocket *theSocket = nullptr;

  //fSocket data member of TCPSocket.
  int osSocket = accept(fFileDesc, (struct sockaddr *) &addr, &size);

  //test osSocket = -1;
  if (osSocket == -1) {
    //take a look at what this error is.
    int acceptError = OSThread::GetErrno();
    if (acceptError == EAGAIN) {
      //If it's EAGAIN, there's nothing on the listen queue right now,
      //so modwatch and return
      this->RequestEvent(EV_RE);
      return;
    }

    //test acceptError = ENFILE;
    //test acceptError = EINTR;
    //test acceptError = ENOENT;

    //if these error gets returned, we're out of file desciptors,
    //the server is going to be failing on sockets, logs, qtgroups and qtuser auth file accesses and movie files. The server is not functional.
    if (acceptError == EMFILE || acceptError == ENFILE) {
#ifndef __Win32__

      QTSSModuleUtils::LogErrorStr(qtssFatalVerbosity,
                                   "Out of File Descriptors. Set max connections lower and check for competing usage from other processes. Exiting.");
#endif

      exit(EXIT_FAILURE);
    } else {
      char errStr[256];
      errStr[sizeof(errStr) - 1] = 0;
      qtss_snprintf(errStr,
                    sizeof(errStr) - 1,
                    "accept error = %d '%s' on socket. Clean up and continue.",
                    acceptError,
                    strerror(acceptError));
      WarnV((acceptError == 0), errStr);

      theTask = this->GetSessionTask(&theSocket);
      if (theTask == nullptr) {
        close(osSocket);
      } else {
        theTask->Signal(Task::kKillEvent); // just clean up the task
      }

      if (theSocket)
        theSocket->fState &= ~kConnected; // turn off connected state

      return;
    }
  }

  theTask = this->GetSessionTask(&theSocket);
  if (theTask == nullptr) {    //this should be a disconnect. do an ioctl call?
    close(osSocket);
    if (theSocket)
      theSocket->fState &= ~kConnected; // turn off connected state
  } else {
    Assert(osSocket != EventContext::kInvalidFileDesc);

    //set options on the socket
    //we are a server, always disable nagle algorithm
    int one = 1;
    int err = ::setsockopt(osSocket,
                           IPPROTO_TCP,
                           TCP_NODELAY,
                           (char *) &one,
                           sizeof(int));
    AssertV(err == 0, OSThread::GetErrno());

    err = ::setsockopt(osSocket,
                       SOL_SOCKET,
                       SO_KEEPALIVE,
                       (char *) &one,
                       sizeof(int));
    AssertV(err == 0, OSThread::GetErrno());

    int sndBufSize = 96L * 1024L;
    err = ::setsockopt(osSocket,
                       SOL_SOCKET,
                       SO_SNDBUF,
                       (char *) &sndBufSize,
                       sizeof(int));
    AssertV(err == 0, OSThread::GetErrno());

    //setup the socket. When there is data on the socket,
    //theTask will get an kReadEvent event
    // TCPSocket::Set 函数通过 getsockname 将 osSocket 描述字对应的 sockaddr 保存到
    // TCPSocket::fLocalAddr,并设置 TCPSocket:: fState |= kBound | kConnected。
    // 同时将 osSocket 保存为 TCPSocket::fFileDesc
    theSocket->Set(osSocket, &addr);
    theSocket->InitNonBlocking(osSocket);
    // 实际上是调用 EventContext::SetTask
    theSocket->SetTask(theTask);
    theSocket->RequestEvent(EV_RE);

    theTask->SetThreadPicker(Task::GetBlockingTaskThreadPicker()); //The Message Task processing threads
  }

  // 如果 RTSPSession、HTTPSession 的连接数超过超过限制,则利用 IdleTaskThread 定时调用
  // Signal 函数,在 TCPListenerSocket::Run 函数里会调用 TCPListenerSocket::ProcessEvent 函数
  // 执行 accept(接收下一个连接)和 RequestEvent(继续申请监听)。
  if (fSleepBetweenAccepts) {
    // We are at our maximum supported sockets
    // slow down so we have time to process the active ones (we will respond with errors or service).
    // wake up and execute again after sleeping. The timer must be reset each time through
    //qtss_printf("TCPListenerSocket slowing down\n");
    this->SetIdleTimer(kTimeBetweenAcceptsInMsec); //sleep 1 second
  } else {
    // sleep until there is a read event outstanding (another client wants to connect)
    //qtss_printf("TCPListenerSocket normal speed\n");
    this->RequestEvent(EV_RE);
  }

  fOutOfDescriptors =
      false; // always false for now  we don't properly handle this elsewhere in the code
}

SInt64 TCPListenerSocket::Run() {
  EventFlags events = this->GetEvents();

  //
  // ProcessEvent cannot be going on when this object gets deleted, because
  // the resolve / release mechanism of EventContext will ensure this thread
  // will block before destructing stuff.
  if (events & Task::kKillEvent)
    return -1;


  //This function will get called when we have run out of file descriptors.
  //All we need to do is check the listen queue to see if the situation has
  //cleared up.
  (void) this->GetEvents();
  this->ProcessEvent(Task::kReadEvent);
  return 0;
}
