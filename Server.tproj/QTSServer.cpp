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
    File:       QTSServer.cpp

    Contains:   Implements object defined in QTSServer.h



*/


#ifndef __Win32__

#include <sys/types.h>
#include <dirent.h>

#endif

#ifndef __Win32__

#include <unistd.h>
#include <grp.h>
#include <pwd.h>

#endif

#include <string>

#include <CF/ArrayObjectDeleter.h>
#include <CF/Net/Socket/SocketUtils.h>

#include "QTSServer.h"

#include "QTSSCallbacks.h"
#include "QTSSModuleUtils.h"

//Compile time modules
#include "QTSSErrorLogModule.h"
#include "QTSSAccessLogModule.h"
#include "QTSSFlowControlModule.h"
#include "QTSSReflectorModule.h"
//#include "EasyCMSModule.h"
//#include "EasyRedisModule.h"

#ifdef PROXYSERVER
#include "QTSSProxyModule.h"
#endif

#include "QTSSPosixFileSysModule.h"
#include "QTSSAccessModule.h"

#if MEMORY_DEBUGGING
#include "QTSSWebDebugModule.h"
#endif

#include "RTSPRequestInterface.h"
#include "RTPSessionInterface.h"
#include "RTSPSession.h"

#include "RTCPTask.h"
#include "QTSSFile.h"

#ifdef _WIN32
#include "CreateDump.h"

LONG CrashHandler_EasyDarwin(EXCEPTION_POINTERS *pException)
{
    SYSTEMTIME	systemTime;
    GetLocalTime(&systemTime);

    char szFile[MAX_PATH] = { 0, };
    sprintf(szFile, TEXT("EasyDarwin_%04d%02d%02d %02d%02d%02d.dmp"), systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
    CreateDumpFile(szFile, pException);

    return EXCEPTION_EXECUTE_HANDLER;		//����ֵEXCEPTION_EXECUTE_HANDLER	EXCEPTION_CONTINUE_SEARCH	EXCEPTION_CONTINUE_EXECUTION
}
#endif

using namespace CF;

// CLASS DEFINITIONS

class RTSPListenerSocket : public Net::TCPListenerSocket {
 public:

  RTSPListenerSocket() {}

  virtual ~RTSPListenerSocket() {}

  //sole job of this object is to implement this function
  virtual Thread::Task *GetSessionTask(Net::TCPSocket **outSocket);

  //check whether the Listener should be idling
  bool OverMaxConnections(UInt32 buffer);

};

class RTPSocketPool : public Net::UDPSocketPool {
 public:

  // Pool of UDP sockets for use by the RTP server

  RTPSocketPool() {}

  ~RTPSocketPool() {}

  virtual Net::UDPSocketPair *ConstructUDPSocketPair();

  virtual void DestructUDPSocketPair(Net::UDPSocketPair *inPair);

  virtual void SetUDPSocketOptions(Net::UDPSocketPair *inPair);
};

char *QTSServer::sPortPrefString = "rtsp_port";
QTSS_Callbacks  QTSServer::sCallbacks;
XMLPrefsParser *QTSServer::sPrefsSource = nullptr;
PrefsSource *QTSServer::sMessagesSource = nullptr;

QTSServer::~QTSServer() {
  //
  // Grab the server mutex. This is to make sure all gets & set values on this
  // object complete before we start deleting stuff
  Core::MutexLocker *serverlocker = new Core::MutexLocker(this->GetServerObjectMutex());

  //
  // Grab the prefs mutex. This is to make sure we can't reread prefs
  // WHILE shutting down, which would cause some weirdness for QTSS API
  // (some modules could get QTSS_RereadPrefs_Role after QTSS_Shutdown, which would be bad)
  Core::MutexLocker *locker = new Core::MutexLocker(this->GetPrefs()->GetMutex());

  QTSS_ModuleState theModuleState;
  theModuleState.curRole = QTSS_Shutdown_Role;
  theModuleState.curTask = nullptr;
  Core::Thread::SetMainThreadData(&theModuleState);

  for (UInt32 x = 0;
       x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kShutdownRole);
       x++)
    (void) QTSServerInterface::GetModule(QTSSModule::kShutdownRole,
                                         x)->CallDispatch(QTSS_Shutdown_Role,
                                                          nullptr);

  Core::Thread::SetMainThreadData(nullptr);

  delete fRTPMap;
  delete fReflectorSessionMap;

  delete fSocketPool;
  delete fSrvrMessages;
  delete locker;
  delete serverlocker;
  delete fSrvrPrefs;
}

// 在 startServer 函数创建 sServer 对象后,该函数被调用。
bool QTSServer::Initialize(XMLPrefsParser *inPrefsSource,
                           PrefsSource *inMessagesSource,
                           UInt16 inPortOverride,
                           bool createListeners,
                           const char *inAbsolutePath) {
  static const UInt32 kRTPSessionMapSize = 2000;
  static const UInt32 kReflectorSessionMapSize = 2000;
  fServerState = qtssFatalErrorState;
  sPrefsSource = inPrefsSource;
  sMessagesSource = inMessagesSource;
  memset(sAbsolutePath, 0, MAX_PATH);
  strcpy(sAbsolutePath, inAbsolutePath);
  this->InitCallbacks();  // 初始化 sCallbacks

  //
  // DICTIONARY INITIALIZATION

  // 调用 SetAttribute Setup all the dictionary stuff
  QTSSModule::Initialize();
  QTSServerPrefs::Initialize();
  QTSSMessages::Initialize();
  RTSPRequestInterface::Initialize();  // 初始化 headerFormatter、noServerInfoHeaderFormatter、调用 SetAttribute。

  RTSPSessionInterface::Initialize();
  RTPSessionInterface::Initialize();
  RTPStream::Initialize();
  RTSPSession::Initialize();  //初始化 sHTTPResponseHeaderPtr、sHTTPResponseNoserverHeaderPtr
  QTSSFile::Initialize();
  QTSSUserProfile::Initialize();

  //
  // STUB SERVER INITIALIZATION
  //
  // Construct stub versions of the prefs and messages dictionaries. We need
  // both of these to initialize the server, but they have to be stubs because
  // their QTSSDictionaryMaps will presumably be modified when modules get loaded.

  // QTSServerPrefs 类用来管理 QTSS 的配置信息
  fSrvrPrefs = new QTSServerPrefs(inPrefsSource, false); // First time, don't write changes to the prefs file
  fSrvrMessages = new QTSSMessages(inMessagesSource);
  QTSSModuleUtils::Initialize(fSrvrMessages,
                              this,
                              QTSServerInterface::GetErrorLogStream());

  //
  // SETUP ASSERT BEHAVIOR
  //
  // Depending on the server preference, we will either break when we hit an
  // assert, or log the assert to the error log
  if (!fSrvrPrefs->ShouldServerBreakOnAssert())
    SetAssertLogger(this->GetErrorLogStream());// the error log stream is our assert logger

  //
  // CREATE GLOBAL OBJECTS
  fSocketPool = new RTPSocketPool();
  fRTPMap = new RefTable(kRTPSessionMapSize);
  fReflectorSessionMap = new RefTable(kReflectorSessionMapSize);

  //
  // Load ERROR LOG module only. This is good in case there is a startup error.

  QTSSModule *theLoggingModule = new QTSSModule("QTSSErrorLogModule");
  (void) theLoggingModule->SetupModule(&sCallbacks, &QTSSErrorLogModule_Main);
  (void) AddModule(theLoggingModule);
  this->BuildModuleRoleArrays();

  //
  // DEFAULT IP ADDRESS & DNS NAME
  // 在 startServer 函数里已经调用了 SocketUtils::Initialize,关于网络及 socket 的一些数据
  // 已经被收集。
  if (!this->SetDefaultIPAddr())
    return false;

  //
  // STARTUP TIME - record it
  fStartupTime_UnixMilli = Core::Time::Milliseconds();
  fGMTOffset = Core::Time::GetGMTOffset();

  //
  // BEGIN LISTENING
  if (createListeners) {
    if (!this->CreateListeners(false, fSrvrPrefs, inPortOverride))
      QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSomePortsFailed, 0);
  }

  if (fNumListeners == 0) {
    if (createListeners)
      QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoPortsSucceeded, 0);
    return false;
  }

  std::string defIP("127.0.0.1");
  auto theLocalAddrStr = Net::SocketUtils::GetIPAddrStr(0);
  if (theLocalAddrStr->Ptr) {
    defIP = std::string(theLocalAddrStr->Ptr, theLocalAddrStr->Len);
  }

  fServerState = qtssStartingUpState;
  return true;
}

void QTSServer::InitModules(QTSS_ServerState inEndState) {
  //
  // LOAD AND INITIALIZE ALL MODULES

  // temporarily set the verbosity on missing prefs when starting up to debug level
  // This keeps all the pref messages being written to the config file from being logged.
  // don't exit until the verbosity level is reset back to the initial prefs.

  LoadModules(fSrvrPrefs);  // 打开 module 所在目录,针对该目录下的所有文件调用 CreateModule 创建模块。
  LoadCompiledInModules();  // 加载一些已编入的模块。
  this->BuildModuleRoleArrays();

  fSrvrPrefs->SetErrorLogVerbosity(
      qtssWarningVerbosity); // turn off info messages while initializing compiled in modules.
  //
  // CREATE MODULE OBJECTS AND READ IN MODULE PREFS

  // Finish setting up modules. Create our final prefs & messages objects,
  // register all global dictionaries, and invoke the modules in their Init roles.
  fStubSrvrPrefs = fSrvrPrefs;
  fStubSrvrMessages = fSrvrMessages;

  fSrvrPrefs = new QTSServerPrefs(sPrefsSource,
                                  true); // Now write changes to the prefs file. First time, we don't
  // because the error messages won't get printed.
  QTSS_ErrorVerbosity serverLevel =
      fSrvrPrefs->GetErrorLogVerbosity(); // get the real prefs verbosity and save it.
  fSrvrPrefs->SetErrorLogVerbosity(qtssWarningVerbosity); // turn off info messages while loading dynamic modules


  fSrvrMessages = new QTSSMessages(sMessagesSource);
  QTSSModuleUtils::Initialize(fSrvrMessages,
                              this,
                              QTSServerInterface::GetErrorLogStream());

  this->SetVal(qtssSvrMessages, &fSrvrMessages, sizeof(fSrvrMessages));
  this->SetVal(qtssSvrPreferences, &fSrvrPrefs, sizeof(fSrvrPrefs));

  //
  // ADD REREAD PREFERENCES SERVICE
  (void) QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
      AddAttribute(QTSS_REREAD_PREFS_SERVICE,
                   (QTSS_AttrFunctionPtr) QTSServer::RereadPrefsService,
                   qtssAttrDataTypeUnknown,
                   qtssAttrModeRead);

  //
  // INVOKE INITIALIZE ROLE
  this->DoInitRole();

  if (fServerState != qtssFatalErrorState)
    fServerState = inEndState; // Server is done starting up!


  fSrvrPrefs->SetErrorLogVerbosity(serverLevel); // reset the server's verbosity back to the original prefs level.
}

void QTSServer::StartTasks() {
  fRTCPTask = new RTCPTask();
  fStatsTask = new RTPStatsUpdaterTask();

  //
  // Start listening
  for (UInt32 x = 0; x < fNumListeners; x++)
    fListeners[x]->RequestEvent(EV_RE);
}

bool QTSServer::SetDefaultIPAddr() {
  //check to make sure there is an available ip interface
  if (Net::SocketUtils::GetNumIPAddrs() == 0) {
    QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgNotConfiguredForIP, 0);
    return false;
  }

  //find out what our default IP addr is & dns name
  UInt32 theNumAddrs = 0;
  UInt32 *theIPAddrs = this->GetRTSPIPAddrs(fSrvrPrefs, &theNumAddrs);
  if (theNumAddrs == 1)
    fDefaultIPAddr = Net::SocketUtils::GetIPAddr(0);
  else
    fDefaultIPAddr = theIPAddrs[0];
  delete[] theIPAddrs;

  for (UInt32 ipAddrIter = 0;
       ipAddrIter < Net::SocketUtils::GetNumIPAddrs();
       ipAddrIter++) {
    if (Net::SocketUtils::GetIPAddr(ipAddrIter) == fDefaultIPAddr) {
      this->SetVal(qtssSvrDefaultDNSName,
                   Net::SocketUtils::GetDNSNameStr(ipAddrIter));
      Assert(this->GetValue(qtssSvrDefaultDNSName)->Ptr != nullptr);
      this->SetVal(qtssSvrDefaultIPAddrStr,
                   Net::SocketUtils::GetIPAddrStr(ipAddrIter));
      Assert(this->GetValue(qtssSvrDefaultDNSName)->Ptr != nullptr);
      break;
    }
  }
  if (this->GetValue(qtssSvrDefaultDNSName)->Ptr == nullptr) {
    //If we've gotten here, what has probably happened is the IP address (explicitly
    //entered as a preference) doesn't exist
    QTSSModuleUtils::LogError(qtssFatalVerbosity,
                              qtssMsgDefaultRTSPAddrUnavail,
                              0);
    return false;
  }
  return true;
}

/**
 *	DESCRIBE:	Add HTTP Port Listening,Total Port Listening=RTSP Port listening + HTTP Port Listening
 *	Author:		Babosa@easydarwin.org
 *	Date:		2015/11/22
 */
bool QTSServer::CreateListeners(bool startListeningNow,
                                QTSServerPrefs *inPrefs,
                                UInt16 inPortOverride) {
  struct PortTracking {
    PortTracking() : fPort(0), fIPAddr(0), fNeedsCreating(true) {}

    UInt16 fPort;
    UInt32 fIPAddr;
    bool fNeedsCreating;
  };

  PortTracking *theRTSPPortTrackers = nullptr;
  UInt32 theTotalRTSPPortTrackers = 0;

  // Get the IP addresses from the pref
  UInt32 theNumAddrs = 0;
  UInt32 *theIPAddrs = this->GetRTSPIPAddrs(inPrefs, &theNumAddrs);
  UInt32 index = 0;

  // Stat Total Num of RTSP Port
  // 参数 inPortOverride 是由 main.cpp 的 thePort 传进来(thePort 是指 RTSP Server 的监听端口,缺
  // 省为 0,可以通过-p 参数来指定)。
  if (inPortOverride != 0) {
    theTotalRTSPPortTrackers =
        theNumAddrs; // one port tracking struct for each IP addr
    theRTSPPortTrackers = new PortTracking[theTotalRTSPPortTrackers];
    for (index = 0; index < theNumAddrs; index++) {
      theRTSPPortTrackers[index].fPort = inPortOverride;
      theRTSPPortTrackers[index].fIPAddr = theIPAddrs[index];
    }
  } else {
    // 根据系统的 IP 地址数目和端口数目,创建 thePortTrackers 数组,记录 IP 地址和端口号。
    UInt32 theNumPorts = 0;
    UInt16 *thePorts = GetRTSPPorts(inPrefs, &theNumPorts);
    theTotalRTSPPortTrackers = theNumAddrs * theNumPorts;
    theRTSPPortTrackers = new PortTracking[theTotalRTSPPortTrackers];

    UInt32 currentIndex = 0;

    for (index = 0; index < theNumAddrs; index++) {
      for (UInt32 portIndex = 0; portIndex < theNumPorts; portIndex++) {
        currentIndex = (theNumPorts * index) + portIndex;

        theRTSPPortTrackers[currentIndex].fPort = thePorts[portIndex];
        theRTSPPortTrackers[currentIndex].fIPAddr = theIPAddrs[index];
      }
    }

    delete[] thePorts;
  }

  delete[] theIPAddrs;
  //
  // Now figure out which of these ports we are *already* listening on.
  // If we already are listening on that port, just move the pointer to the
  // listener over to the new array
  Net::TCPListenerSocket **newListenerArray =
      new Net::TCPListenerSocket *[theTotalRTSPPortTrackers];
  UInt32 curPortIndex = 0;

  // RTSPPortTrackers check
  // 将 thePortTrackers 数组的每一项和 fListeners 的比较,如果发现有 IP 地址和端口号均相同的一
  // 项,将 thePortTrackers 相应项的 fNeedsCreating 设为 false,并将 fListeners 这一项的指针保存
  // 到新创建的 newListenerArray 数组。
  for (UInt32 count = 0; count < theTotalRTSPPortTrackers; count++) {
    for (UInt32 count2 = 0; count2 < fNumListeners; count2++) {
      if ((fListeners[count2]->GetLocalPort() == theRTSPPortTrackers[count].fPort)
          && (fListeners[count2]->GetLocalAddr() == theRTSPPortTrackers[count].fIPAddr)) {
        theRTSPPortTrackers[count].fNeedsCreating = false;
        newListenerArray[curPortIndex++] = fListeners[count2];
        Assert(curPortIndex <= theTotalRTSPPortTrackers);
        break;
      }
    }
  }

  // 重新遍历 thePortTrackers 数组,处理 fNeedsCreating 为 true 的项:
  // Create any new <RTSP> listeners we need
  for (UInt32 count3 = 0; count3 < theTotalRTSPPortTrackers; count3++) {
    if (theRTSPPortTrackers[count3].fNeedsCreating) {
      // 创建 RTSPListenerSocket 类对象,并保存到 newListenerArray 数组,并调用该对象的
      // Initialize 成员函数。
      newListenerArray[curPortIndex] = new RTSPListenerSocket();
      // Initialize 函数实际上是 TCPListenerSocket::Initialize,在这个函数里会对 socket(流套接字)执行
      // open、bind、listen 等操作,注意这里绑定的 IP 地址缺省为 0<在配置文件里对应 bind_ip_addr 项>,表示监
      // 听所有地址的连接。绑定的 Port 缺省为 7070、554、8000、8001<在配置文件里对应 rtsp_port 项>
      QTSS_Error err = newListenerArray[curPortIndex]
          ->Initialize(theRTSPPortTrackers[count3].fIPAddr,
                       theRTSPPortTrackers[count3].fPort);

      char thePortStr[20];
      s_sprintf(thePortStr, "%hu", theRTSPPortTrackers[count3].fPort);

      //
      // If there was an error creating this listener, destroy it and log an error
      if ((startListeningNow) && (err != QTSS_NoErr))
        delete newListenerArray[curPortIndex];

      if (err == EADDRINUSE) {
        QTSSModuleUtils::LogError(qtssWarningVerbosity,
                                  qtssListenPortInUse,
                                  0,
                                  thePortStr);
      } else if (err == EACCES) {
        QTSSModuleUtils::LogError(qtssWarningVerbosity,
                                  qtssListenPortAccessDenied,
                                  0,
                                  thePortStr);
      } else if (err != QTSS_NoErr) {
        QTSSModuleUtils::LogError(qtssWarningVerbosity,
                                  qtssListenPortError,
                                  0,
                                  thePortStr);
      } else {
        //
        // This listener was successfully created.
        // 如果 CreateListeners 的参数 startListeningNow 为 true,则 start listening
        if (startListeningNow)
          newListenerArray[curPortIndex]->RequestEvent(EV_RE);
        curPortIndex++;
      }
    }
  }

  //
  // Kill any listeners that we no longer need
  // 把 fListeners 里包含有但是 newListenerArray 里没有(即不再需要的)的监听撤销掉。
  for (UInt32 count4 = 0; count4 < fNumListeners; count4++) {
    bool deleteThisOne = true;

    for (UInt32 count5 = 0; count5 < curPortIndex; count5++) {
      if (newListenerArray[count5] == fListeners[count4])
        deleteThisOne = false;
    }

    if (deleteThisOne)
      fListeners[count4]->Signal(Thread::Task::kKillEvent);
  }

  //
  // Finally, make our server attributes and fListener privy to the new...
  //最后,设置属性(注意这里调用的是 SetValue 函数)
  fListeners = newListenerArray;
  fNumListeners = curPortIndex;
  UInt32 portIndex = 0;

  for (UInt32 count6 = 0; count6 < fNumListeners; count6++) {
    if (fListeners[count6]->GetLocalAddr() != INADDR_LOOPBACK) {
      UInt16 thePort = fListeners[count6]->GetLocalPort();
      (void) this->SetValue(qtssSvrRTSPPorts,
                            portIndex,
                            &thePort,
                            sizeof(thePort),
                            QTSSDictionary::kDontObeyReadOnly);
      portIndex++;
    }
  }
  this->SetNumValues(qtssSvrRTSPPorts, portIndex);

  delete[] theRTSPPortTrackers;
  return (fNumListeners > 0);
}

UInt32 *QTSServer::GetRTSPIPAddrs(QTSServerPrefs *inPrefs,
                                  UInt32 *outNumAddrsPtr) {
  UInt32 numAddrs = inPrefs->GetNumValues(qtssPrefsRTSPIPAddr);
  UInt32 *theIPAddrArray = nullptr;

  if (numAddrs == 0) {
    *outNumAddrsPtr = 1;
    theIPAddrArray = new UInt32[1];
    theIPAddrArray[0] = INADDR_ANY;
  } else {
    theIPAddrArray = new UInt32[numAddrs + 1];
    UInt32 arrIndex = 0;

    for (UInt32 theIndex = 0; theIndex < numAddrs; theIndex++) {
      // Get the ip addr out of the prefs dictionary
      QTSS_Error theErr = QTSS_NoErr;

      char *theIPAddrStr = nullptr;
      theErr = inPrefs->GetValueAsString(qtssPrefsRTSPIPAddr,
                                         theIndex,
                                         &theIPAddrStr);
      if (theErr != QTSS_NoErr) {
        delete[] theIPAddrStr;
        break;
      }

      UInt32 theIPAddr = 0;
      if (theIPAddrStr != nullptr) {
        theIPAddr = Net::SocketUtils::ConvertStringToAddr(theIPAddrStr);
        delete[] theIPAddrStr;

        if (theIPAddr != 0)
          theIPAddrArray[arrIndex++] = theIPAddr;
      }
    }

    if ((numAddrs == 1) && (arrIndex == 0))
      theIPAddrArray[arrIndex++] = INADDR_ANY;
    else
      theIPAddrArray[arrIndex++] = INADDR_LOOPBACK;

    *outNumAddrsPtr = arrIndex;
  }

  return theIPAddrArray;
}

UInt16 *QTSServer::GetRTSPPorts(QTSServerPrefs *inPrefs,
                                UInt32 *outNumPortsPtr) {
  *outNumPortsPtr = inPrefs->GetNumValues(qtssPrefsRTSPPorts);

  if (*outNumPortsPtr == 0)
    return nullptr;

  UInt16 *thePortArray = new UInt16[*outNumPortsPtr];

  for (UInt32 theIndex = 0; theIndex < *outNumPortsPtr; theIndex++) {
    // Get the ip addr out of the prefs dictionary
    UInt32 theLen = sizeof(UInt16);
    QTSS_Error theErr = QTSS_NoErr;
    theErr = inPrefs->GetValue(qtssPrefsRTSPPorts,
                               theIndex,
                               &thePortArray[theIndex],
                               &theLen);
    Assert(theErr == QTSS_NoErr);
  }

  return thePortArray;
}

bool QTSServer::SetupUDPSockets() {
  // function finds all IP addresses on this machine, and binds 1 RTP / RTCP
  // socket pair to a port pair on each address.

  UInt32 theNumAllocatedPairs = 0;
  for (UInt32 theNumPairs = 0;
       theNumPairs < Net::SocketUtils::GetNumIPAddrs();
       theNumPairs++) {
    // 在 QTSServer::Initialize 函数里,fSocketPool=new RTPSocketPool();
    // RTPSocketPool 是 UDPSocketPool 的继承类,它们的构建函数均为空。
    // 基于同一个 ip,创建一个 socket 对。
    Net::UDPSocketPair *thePair = fSocketPool
        ->CreateUDPSocketPair(Net::SocketUtils::GetIPAddr(theNumPairs), 0);
    if (thePair != nullptr) {
      theNumAllocatedPairs++;
      // 申请监听这两个 socket 端口
      thePair->GetSocketA()->RequestEvent(EV_RE);
      thePair->GetSocketB()->RequestEvent(EV_RE);
    }
  }
  //only return an error if we couldn't allocate ANY pairs of sockets
  if (theNumAllocatedPairs == 0) {
    fServerState = qtssFatalErrorState; // also set the state to fatal error
    return false;
  }
  return true;
}

bool QTSServer::SwitchPersonality() {
#ifndef __Win32__  //not supported
  CharArrayDeleter runGroupName(fSrvrPrefs->GetRunGroupName());
  CharArrayDeleter runUserName(fSrvrPrefs->GetRunUserName());

  int groupID = 0;

  if (::strlen(runGroupName.GetObject()) > 0) {
    struct group *gr = ::getgrnam(runGroupName.GetObject());
    if (gr == nullptr || ::setgid(gr->gr_gid) == -1) {
      char buffer[kErrorStrSize];

      QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgCannotSetRunGroup, 0,
                                runGroupName.GetObject(),
                                s_strerror(Core::Thread::GetErrno(),
                                              buffer,
                                              sizeof(buffer)));
      return false;
    }
    groupID = gr->gr_gid;
  }

  if (::strlen(runUserName.GetObject()) > 0) {
    struct passwd *pw = ::getpwnam(runUserName.GetObject());

#if __MacOSX__
    if (pw != nullptr && groupID != 0) //call initgroups before doing a setuid
        (void) initgroups(runUserName.GetObject(), groupID);
#endif

    if (pw == nullptr || ::setuid(pw->pw_uid) == -1) {
      QTSSModuleUtils::LogError(qtssFatalVerbosity,
                                qtssMsgCannotSetRunUser,
                                0,
                                runUserName.GetObject(),
                                strerror(Core::Thread::GetErrno()));
      return false;
    }
  }

#endif
  return true;
}

void QTSServer::LoadCompiledInModules() {
#ifndef DSS_DYNAMIC_MODULES_ONLY
  // MODULE DEVELOPERS SHOULD ADD THE FOLLOWING THREE LINES OF CODE TO THIS
  // FUNCTION IF THEIR MODULE IS BEING COMPILED INTO THE SERVER.
  //
  // QTSSModule* myModule = new QTSSModule("__MODULE_NAME__");
  // (void)myModule->Initialize(&sCallbacks, &__MODULE_MAIN_ROUTINE__);
  // (void)AddModule(myModule);

  QTSSModule *theReflectorModule = new QTSSModule("QTSSReflectorModule");
  (void) theReflectorModule->SetupModule(&sCallbacks,
                                         &QTSSReflectorModule_Main);
  (void) AddModule(theReflectorModule);

  QTSSModule *theAccessLog = new QTSSModule("QTSSAccessLogModule");
  (void) theAccessLog->SetupModule(&sCallbacks, &QTSSAccessLogModule_Main);
  (void) AddModule(theAccessLog);

  QTSSModule *theFlowControl = new QTSSModule("QTSSFlowControlModule");
  (void) theFlowControl->SetupModule(&sCallbacks, &QTSSFlowControlModule_Main);
  (void) AddModule(theFlowControl);

  QTSSModule *theFileSysModule = new QTSSModule("QTSSPosixFileSysModule");
  (void) theFileSysModule->SetupModule(&sCallbacks,
                                       &QTSSPosixFileSysModule_Main);
  (void) AddModule(theFileSysModule);

//    if (this->GetPrefs()->CloudPlatformEnabled()) {
//        QTSSModule *theCMSModule = new QTSSModule("EasyCMSModule");
//        (void) theCMSModule->SetupModule(&sCallbacks, &EasyCMSModule_Main);
//        (void) AddModule(theCMSModule);
//
//        QTSSModule *theRedisModule = new QTSSModule("EasyRedisModule");
//        (void) theRedisModule->SetupModule(&sCallbacks, &EasyRedisModule_Main);
//        (void) AddModule(theRedisModule);
//    }

#if MEMORY_DEBUGGING
  QTSSModule* theWebDebug = new QTSSModule("QTSSWebDebugModule");
  (void)theWebDebug->SetupModule(&sCallbacks, &QTSSWebDebugModule_Main);
  (void)AddModule(theWebDebug);
#endif

#ifdef __MacOSX__
  QTSSModule* theQTSSDSAuthModule = new QTSSModule("QTSSDSAuthModule");
  (void)theQTSSDSAuthModule->SetupModule(&sCallbacks, &QTSSDSAuthModule_Main);
  (void)AddModule(theQTSSDSAuthModule);
#endif

  QTSSModule *theQTACCESSmodule = new QTSSModule("QTSSAccessModule");
  (void) theQTACCESSmodule->SetupModule(&sCallbacks, &QTSSAccessModule_Main);
  (void) AddModule(theQTACCESSmodule);

#endif //DSS_DYNAMIC_MODULES_ONLY

#ifdef PROXYSERVER
  QTSSModule* theProxyModule = new QTSSModule("QTSSProxyModule");
  (void)theProxyModule->SetupModule(&sCallbacks, &QTSSProxyModule_Main);
  (void)AddModule(theProxyModule);
#endif

#ifdef _WIN32
  SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler_EasyDarwin));
#endif

}

void QTSServer::InitCallbacks() {
  sCallbacks.addr[kNewCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_New;
  sCallbacks.addr[kDeleteCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Delete;
  sCallbacks.addr[kMillisecondsCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Milliseconds;
  sCallbacks.addr[kConvertToUnixTimeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_ConvertToUnixTime;

  sCallbacks.addr[kAddRoleCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_AddRole;
  sCallbacks.addr[kCreateObjectTypeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_CreateObjectType;
  sCallbacks.addr[kAddAttributeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_AddAttribute;
  sCallbacks.addr[kIDForTagCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_IDForAttr;
  sCallbacks.addr[kGetAttributePtrByIDCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetValuePtr;
  sCallbacks.addr[kGetAttributeByIDCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetValue;
  sCallbacks.addr[kSetAttributeByIDCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_SetValue;
  sCallbacks.addr[kCreateObjectValueCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_CreateObject;
  sCallbacks.addr[kGetNumValuesCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetNumValues;

  sCallbacks.addr[kWriteCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Write;
  sCallbacks.addr[kWriteVCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_WriteV;
  sCallbacks.addr[kFlushCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Flush;
  sCallbacks.addr[kReadCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Read;
  sCallbacks.addr[kSeekCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Seek;
  sCallbacks.addr[kAdviseCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Advise;

  sCallbacks.addr[kAddServiceCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_AddService;
  sCallbacks.addr[kIDForServiceCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_IDForService;
  sCallbacks.addr[kDoServiceCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_DoService;

  sCallbacks.addr[kSendRTSPHeadersCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_SendRTSPHeaders;
  sCallbacks.addr[kAppendRTSPHeadersCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_AppendRTSPHeader;
  sCallbacks.addr[kSendStandardRTSPCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_SendStandardRTSPResponse;

  sCallbacks.addr[kAddRTPStreamCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_AddRTPStream;
  sCallbacks.addr[kPlayCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Play;
  sCallbacks.addr[kPauseCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Pause;
  sCallbacks.addr[kTeardownCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Teardown;
  sCallbacks.addr[kRefreshTimeOutCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_RefreshTimeOut;

  sCallbacks.addr[kRequestEventCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_RequestEvent;
  sCallbacks.addr[kSetIdleTimerCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_SetIdleTimer;
  sCallbacks.addr[kSignalStreamCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_SignalStream;

  sCallbacks.addr[kOpenFileObjectCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_OpenFileObject;
  sCallbacks.addr[kCloseFileObjectCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_CloseFileObject;

  sCallbacks.addr[kCreateSocketStreamCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_CreateStreamFromSocket;
  sCallbacks.addr[kDestroySocketStreamCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_DestroySocketStream;

  sCallbacks.addr[kAddStaticAttributeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_AddStaticAttribute;
  sCallbacks.addr[kAddInstanceAttributeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_AddInstanceAttribute;
  sCallbacks.addr[kRemoveInstanceAttributeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_RemoveInstanceAttribute;

  sCallbacks.addr[kGetAttrInfoByIndexCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetAttrInfoByIndex;
  sCallbacks.addr[kGetAttrInfoByNameCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetAttrInfoByName;
  sCallbacks.addr[kGetAttrInfoByIDCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetAttrInfoByID;
  sCallbacks.addr[kGetNumAttributesCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetNumAttributes;

  sCallbacks.addr[kGetValueAsStringCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_GetValueAsString;
  sCallbacks.addr[kTypeToTypeStringCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_TypeToTypeString;
  sCallbacks.addr[kTypeStringToTypeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_TypeStringToType;
  sCallbacks.addr[kStringToValueCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_StringToValue;
  sCallbacks.addr[kValueToStringCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_ValueToString;

  sCallbacks.addr[kRemoveValueCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_RemoveValue;

  sCallbacks.addr[kRequestGlobalLockCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_RequestLockedCallback;
  sCallbacks.addr[kIsGlobalLockedCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_IsGlobalLocked;
  sCallbacks.addr[kUnlockGlobalLock] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_UnlockGlobalLock;

  sCallbacks.addr[kAuthenticateCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Authenticate;
  sCallbacks.addr[kAuthorizeCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_Authorize;

  sCallbacks.addr[kLockObjectCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_LockObject;
  sCallbacks.addr[kUnlockObjectCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_UnlockObject;
  sCallbacks.addr[kSetAttributePtrCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_SetValuePtr;

  sCallbacks.addr[kSetIntervalRoleTimerCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_SetIdleRoleTimer;

  sCallbacks.addr[kLockStdLibCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_LockStdLib;
  sCallbacks.addr[kUnlockStdLibCallback] = (QTSS_CallbackProcPtr) QTSSCallbacks::QTSS_UnlockStdLib;

}

void QTSServer::LoadModules(QTSServerPrefs *inPrefs) {
  // Fetch the name of the module directory and open it.
  CharArrayDeleter theModDirName(inPrefs->GetModuleDirectory());

#ifdef __Win32__
  // NT doesn't seem to have support for the POSIX directory parsing APIs.
  OSCharArrayDeleter theLargeModDirName(new char[::strlen(theModDirName.GetObject()) + 3]);
  ::strcpy(theLargeModDirName.GetObject(), theModDirName.GetObject());
  ::strcat(theLargeModDirName.GetObject(), "\\*");

  WIN32_FIND_DATA theFindData;
  HANDLE theSearchHandle = ::FindFirstFile(theLargeModDirName.GetObject(), &theFindData);

  if (theSearchHandle == INVALID_HANDLE_VALUE)
  {
      QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoModuleFolder, 0);
      return;
  }

  while (theSearchHandle != INVALID_HANDLE_VALUE)
  {
      this->CreateModule(theModDirName.GetObject(), theFindData.cFileName);

      if (!::FindNextFile(theSearchHandle, &theFindData))
      {
          ::FindClose(theSearchHandle);
          theSearchHandle = INVALID_HANDLE_VALUE;
      }
  }
#else

  // POSIX version
  // opendir mallocs memory for DIR* so call closedir to free the allocated memory
  DIR *theDir = ::opendir(theModDirName.GetObject());
  if (theDir == nullptr) {
    QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoModuleFolder, 0);
    return;
  }

  while (true) {
    // Iterate over each file in the directory, attempting to construct
    // a module object from that file.

    struct dirent *theFile = ::readdir(theDir);
    if (theFile == nullptr)
      break;

    this->CreateModule(theModDirName.GetObject(), theFile->d_name);
  }

  (void) ::closedir(theDir);

#endif
}

void QTSServer::CreateModule(char *inModuleFolderPath, char *inModuleName) {
  // 通过创建一个模块对象、调用该模块的 SetupModule 函数、调用 QTSServer::AddModule 函数这三个步
  // 骤来完成创建加载模块的过程。

  // Ignore these silly directory names

  if (::strcmp(inModuleName, ".") == 0)
    return;
  if (::strcmp(inModuleName, "..") == 0)
    return;
  if (::strlen(inModuleName) == 0)
    return;
  if (*inModuleName == '.')
    return; // Fix 2572248. Do not attempt to load '.' files as modules at all

  //
  // Construct a full path to this module
  UInt32 totPathLen = ::strlen(inModuleFolderPath) + ::strlen(inModuleName);
  CharArrayDeleter theModPath(new char[totPathLen + 4]);
  ::strcpy(theModPath.GetObject(), inModuleFolderPath);
  ::strcat(theModPath.GetObject(), kPathDelimiterString);
  ::strcat(theModPath.GetObject(), inModuleName);

  //
  // Construct a QTSSModule object, and attempt to initialize the module
  QTSSModule
      *theNewModule = new QTSSModule(inModuleName, theModPath.GetObject());
  QTSS_Error theErr = theNewModule->SetupModule(&sCallbacks);

  if (theErr != QTSS_NoErr) {
    QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgBadModule, theErr,
                              inModuleName);
    delete theNewModule;
  }
    //
    // If the module was successfully initialized, add it to our module queue
  else if (!this->AddModule(theNewModule)) {
    QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgRegFailed, theErr,
                              inModuleName);
    delete theNewModule;
  }
}

bool QTSServer::AddModule(QTSSModule *inModule) {
  Assert(inModule->IsInitialized());

  // Prepare to invoke the module's Register role. Setup the Register param block
  QTSS_ModuleState theModuleState;

  theModuleState.curModule = inModule;
  theModuleState.curRole = QTSS_Register_Role;
  theModuleState.curTask = nullptr;
  Core::Thread::SetMainThreadData(&theModuleState);

  // Currently we do nothing with the module name
  QTSS_RoleParams theRegParams;
  theRegParams.regParams.outModuleName[0] = 0;

  // If the module returns an error from the QTSS_Register role, don't put it anywhere
  if (inModule->CallDispatch(QTSS_Register_Role, &theRegParams) != QTSS_NoErr) {
    //Log
    char msgStr[2048];
    char *moduleName = nullptr;
    (void) inModule->GetValueAsString(qtssModName, 0, &moduleName);
    s_snprintf(msgStr,
                  sizeof(msgStr),
                  "Loading Module [%s] Failed!",
                  moduleName);
    delete moduleName;
    QTSServerInterface::LogError(qtssMessageVerbosity, msgStr);
    return false;
  }

  Core::Thread::SetMainThreadData(nullptr);

  //
  // Update the module name to reflect what was returned from the register role
  theRegParams.regParams.outModuleName[QTSS_MAX_MODULE_NAME_LENGTH - 1] = 0;
  if (theRegParams.regParams.outModuleName[0] != 0)
    inModule->SetValue(qtssModName, 0, theRegParams.regParams.outModuleName,
                       ::strlen(theRegParams.regParams.outModuleName), false);

  //
  // Give the module object a prefs dictionary. Instance attributes are allowed for these objects.
  QTSSPrefs
      *thePrefs = new QTSSPrefs(sPrefsSource,
                                inModule->GetValue(qtssModName),
                                QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModulePrefsDictIndex),
                                true);
  thePrefs->RereadPreferences();
  inModule->SetPrefsDict(thePrefs);

  //
  // Add this module to the array of module (dictionaries)
  UInt32 theNumModules = this->GetNumValues(qtssSvrModuleObjects);
  QTSS_Error theErr = this->SetValue(qtssSvrModuleObjects,
                                     theNumModules,
                                     &inModule,
                                     sizeof(QTSSModule *),
                                     QTSSDictionary::kDontObeyReadOnly);
  Assert(theErr == QTSS_NoErr);

  //
  // Add this module to the module queue
  sModuleQueue.EnQueue(inModule->GetQueueElem());

  return true;
}

void QTSServer::BuildModuleRoleArrays() {
  QueueIter theIter(&sModuleQueue);
  QTSSModule *theModule = nullptr;

  // Make sure these variables are cleaned out in case they've already been inited.

  DestroyModuleRoleArrays();

  // Loop through all the roles of all the modules, recording the number of
  // modules in each role, and also recording which modules are doing what.

  for (UInt32 x = 0; x < QTSSModule::kNumRoles; x++) {
    sNumModulesInRole[x] = 0;
    for (theIter.Reset(); !theIter.IsDone(); theIter.Next()) {
      theModule = (QTSSModule *) theIter.GetCurrent()->GetEnclosingObject();
      if (theModule->RunsInRole(x))
        sNumModulesInRole[x] += 1;
    }

    if (sNumModulesInRole[x] > 0) {
      UInt32 moduleIndex = 0;
      sModuleArray[x] = new QTSSModule *[sNumModulesInRole[x] + 1];
      for (theIter.Reset(); !theIter.IsDone(); theIter.Next()) {
        theModule = (QTSSModule *) theIter.GetCurrent()->GetEnclosingObject();
        if (theModule->RunsInRole(x)) {
          sModuleArray[x][moduleIndex] = theModule;
          moduleIndex++;
        }
      }
    }
  }
}

void QTSServer::DestroyModuleRoleArrays() {
  for (UInt32 x = 0; x < QTSSModule::kNumRoles; x++) {
    sNumModulesInRole[x] = 0;
    if (sModuleArray[x] != nullptr)
      delete[] sModuleArray[x];
    sModuleArray[x] = nullptr;
  }
}

void QTSServer::DoInitRole() {
  QTSS_RoleParams theInitParams;
  theInitParams.initParams.inServer = this;
  theInitParams.initParams.inPrefs = fSrvrPrefs;
  theInitParams.initParams.inMessages = fSrvrMessages;
  theInitParams.initParams.inErrorLogStream = &sErrorLogStream;

  QTSS_ModuleState theModuleState;
  theModuleState.curRole = QTSS_Initialize_Role;
  theModuleState.curTask = nullptr;
  Core::Thread::SetMainThreadData(&theModuleState);

  //
  // Add the OPTIONS method as the one method the server handles by default (it handles
  // it internally). Modules that handle other RTSP methods will add
  QTSS_RTSPMethod theOptionsMethod = qtssOptionsMethod;
  (void) this->SetValue(qtssSvrHandledMethods,
                        0,
                        &theOptionsMethod,
                        sizeof(theOptionsMethod));


  // For now just disable the SetParameter to be compatible with Real.  It should really be removed
  // only for clients that have problems with their SetParameter implementations like (Real Players).
  // At the moment it isn't necesary to add the option.
  //   QTSS_RTSPMethod	theSetParameterMethod = qtssSetParameterMethod;
  //    (void)this->SetValue(qtssSvrHandledMethods, 0, &theSetParameterMethod, sizeof(theSetParameterMethod));

  // 对于那些支持 Initialize Role 的模块,通过它们的 CallDispatch 函数来调用具体的 Initialize
  // 函数。实际上这也给各个模块(包括用户自定义的模块)一个执行初始化动作的机会。
  for (UInt32 x = 0;
       x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kInitializeRole);
       x++) {
    QTSSModule *theModule =
        QTSServerInterface::GetModule(QTSSModule::kInitializeRole, x);
    theInitParams.initParams.inModule = theModule;
    theModuleState.curModule = theModule;
    QTSS_Error
        theErr = theModule->CallDispatch(QTSS_Initialize_Role, &theInitParams);

    if (theErr != QTSS_NoErr) {
      // If the module reports an error when initializing itself,
      // delete the module and pretend it was never there.
      QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgInitFailed, theErr,
                                theModule->GetValue(qtssModName)->Ptr);

      sModuleQueue.Remove(theModule->GetQueueElem());
      delete theModule;
    }
  }
  this->SetupPublicHeader();

  Core::Thread::SetMainThreadData(nullptr);
}

void QTSServer::SetupPublicHeader() {
  //
  // After the Init role, all the modules have reported the methods that they handle.
  // So, we can prune this attribute for duplicates, and construct a string to use in the
  // Public: header of the OPTIONS response
  // 将各个模块登记的支持方法信息汇总一起。在 RTSPSession 处理 OptionMethod 时,会将这些信
  // 息发送出去。
  QTSS_RTSPMethod *theMethod = nullptr;
  UInt32 theLen = 0;

  bool theUniqueMethods[qtssNumMethods + 1];
  ::memset(theUniqueMethods, 0, sizeof(theUniqueMethods));

  for (UInt32 y = 0; this->GetValuePtr(qtssSvrHandledMethods,
                                       y,
                                       (void **) &theMethod,
                                       &theLen) == QTSS_NoErr; y++)
    theUniqueMethods[*theMethod] = true;

  // Rewrite the qtssSvrHandledMethods, eliminating any duplicates that modules may have introduced
  UInt32 uniqueMethodCount = 0;
  for (QTSS_RTSPMethod z = 0; z < qtssNumMethods; z++) {
    if (theUniqueMethods[z])
      this->SetValue(qtssSvrHandledMethods,
                     uniqueMethodCount++,
                     &z,
                     sizeof(QTSS_RTSPMethod));
  }
  this->SetNumValues(qtssSvrHandledMethods, uniqueMethodCount);

  // Format a text string for the Public: header
  ResizeableStringFormatter theFormatter(nullptr, 0);

  for (UInt32 a = 0; this->GetValuePtr(qtssSvrHandledMethods,
                                       a,
                                       (void **) &theMethod,
                                       &theLen) == QTSS_NoErr; a++) {
    sPublicHeaderFormatter.Put(RTSPProtocol::GetMethodString(*theMethod));
    sPublicHeaderFormatter.Put(", ");
  }
  sPublicHeaderStr.Ptr = sPublicHeaderFormatter.GetBufPtr();
  sPublicHeaderStr.Len =
      sPublicHeaderFormatter.GetBytesWritten() - 2; //trunc the last ", "
}

Thread::Task *RTSPListenerSocket::GetSessionTask(Net::TCPSocket **outSocket) {
  Assert(outSocket != nullptr);

  // when the server is behing a round robin DNS, the client needs to know the IP address ot the server
  // so that it can direct the "POST" half of the connection to the same machine when tunnelling RTSP thru HTTP
  bool doReportHTTPConnectionAddress =
      QTSServerInterface::GetServer()->GetPrefs()->GetDoReportHTTPConnectionAddress();

  RTSPSession *theTask = new RTSPSession(doReportHTTPConnectionAddress);
  // 返回 fSocket 成员
  *outSocket = theTask->GetSocket();  // out socket is not attached to a unix socket yet.

  // 根据配置文件中的 maximum_connections(缺省为 1000)、fNumRTSPSessions、
  // fNumRTSPHTTPSessions(这两个变量值在 RTSPSession 的构建、析构函数、
  // PreFilterForHTTPProxyTunnel 函数里变化)来计算是否超过极限。
  if (this->OverMaxConnections(0))
    this->SlowDown();
  else
    this->RunNormal();

  return theTask;
}

bool RTSPListenerSocket::OverMaxConnections(UInt32 buffer) {
  QTSServerInterface *theServer = QTSServerInterface::GetServer();
  SInt32 maxConns = theServer->GetPrefs()->GetMaxConnections();
  bool overLimit = false;

  if (maxConns > -1) // limit connections
  {
    maxConns += buffer;
    if ((theServer->GetNumRTPSessions() > (UInt32) maxConns)
        ||
            (theServer->GetNumRTSPSessions()
                + theServer->GetNumRTSPHTTPSessions() > (UInt32) maxConns)
        ) {
      overLimit = true;
    }
  }
  return overLimit;

}

Net::UDPSocketPair *RTPSocketPool::ConstructUDPSocketPair() {
  Thread::Task *theTask = ((QTSServer *) QTSServerInterface::GetServer())->fRTCPTask;

  // construct a pair of UDP sockets, the lower one for RTP data (outgoing only, no demuxer
  // necessary), and one for RTCP data (incoming, so definitely need a demuxer).
  // These are nonblocking sockets that DON'T receive events (we are going to poll for data)
  // They do receive events - we don't poll from them anymore
  // 在 RTCPTask 的 Run 函数里,会寻找带有 Demuxer 的 socket,将接收到的数据交由 Demuxer 相联
  // 系的 Task 进行处理。
  // 注意这里传入的是 fRTCPTask!
  return new Net::UDPSocketPair(
      new Net::UDPSocket(theTask, Net::Socket::kNonBlockingSocketType),
      new Net::UDPSocket(theTask, Net::UDPSocket::kWantsDemuxer
          | Net::Socket::kNonBlockingSocketType));
}

void RTPSocketPool::DestructUDPSocketPair(Net::UDPSocketPair *inPair) {
  delete inPair->GetSocketA();
  delete inPair->GetSocketB();
  delete inPair;
}

void RTPSocketPool::SetUDPSocketOptions(Net::UDPSocketPair *inPair) {
  // Apparently the socket buffer size matters even though this is UDP and being
  // used for sending... on UNIX typically the socket buffer size doesn't matter because the
  // packet goes right down to the driver. On Win32 and linux, unless this is really big, we get packet loss.
  // Socket A 用来发送 RTP data,通过 setsockopt 将发送 buf 设为 256K
  inPair->GetSocketA()->SetSocketBufSize(256 * 1024);

  // Socket B 用来接收 RTCP data,注意这几个 socket 端口的 buffer size:
  // TCPListenerSocket 的 rcv buf size 为 96K、TCPListenerSocket accept 后返回的 socket snd
  // buf size 为 256K。

  //
  // Always set the Rcv buf size for the RTCP sockets. This is important because the
  // server is going to be getting many many acks.
  UInt32 theRcvBufSize =
      QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK();

  // 获得用于 RTCP 的 rcv buf size 大小,缺省为 768K,在配置文件中对应 rtcp_rcv_buf_size 项

  //
  // In case the rcv buf size is too big for the system, retry, dividing the requested size by 2.
  // Until it works, or until some minimum value is reached.
  OS_Error theErr = EINVAL;
  while ((theErr != OS_NoErr) && (theRcvBufSize > 32)) {
    theErr = inPair->GetSocketB()->SetSocketRcvBufSize(theRcvBufSize * 1024);
    if (theErr != OS_NoErr)
      theRcvBufSize >>= 1;
  }

  //
  // Report an error if we couldn't set the socket buffer size the user requested
  if (theRcvBufSize
      != QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK()) {
    char theRcvBufSizeStr[20];
    s_sprintf(theRcvBufSizeStr, "%"   _U32BITARG_   "", theRcvBufSize);
    //
    // For now, do not log an error, though we should enable this in the future.
    //QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSockBufSizesTooLarge, theRcvBufSizeStr);
  }
}

QTSS_Error QTSServer::RereadPrefsService(QTSS_ServiceFunctionArgsPtr /*inArgs*/) {
  //
  // This function can only be called safely when the server is completely running.
  // Ensuring this is a bit complicated because of preemption. Here's how it's done...

  QTSServerInterface *theServer = QTSServerInterface::GetServer();

  // This is to make sure this function isn't being called before the server is
  // completely started up.
  if ((theServer == nullptr)
      || (theServer->GetServerState() != qtssRunningState))
    return QTSS_OutOfState;

  // Because the server must have started up, and because this object always stays
  // around (until the process dies), we can now safely get this object.
  QTSServerPrefs *thePrefs = theServer->GetPrefs();

  // Grab the prefs mutex. We want to make sure that calls to RereadPrefsService
  // are serialized. This also prevents the server from shutting down while in
  // this function, because the QTSServer destructor grabs this mutex as well.
  Core::MutexLocker locker(thePrefs->GetMutex());

  // Finally, check the server state again. The state may have changed
  // to qtssShuttingDownState or qtssFatalErrorState in this time, though
  // at this point we have the prefs mutex, so we are guarenteed that the
  // server can't actually shut down anymore
  if (theServer->GetServerState() != qtssRunningState)
    return QTSS_OutOfState;

  // Ok, we're ready to reread preferences now.

  //
  // Reread preferences
  sPrefsSource->Parse();
  thePrefs->RereadServerPreferences(true);

  {
    //
    // Update listeners, ports, and IP addrs.
    Core::MutexLocker locker(theServer->GetServerObjectMutex());
    (void) ((QTSServer *) theServer)->SetDefaultIPAddr();
    (void) ((QTSServer *) theServer)->CreateListeners(true, thePrefs, 0);
  }

  // Delete all the streams
  QTSSModule **theModule = nullptr;
  UInt32 theLen = 0;

  for (int y = 0;
       QTSServerInterface::GetServer()->GetValuePtr(qtssSvrModuleObjects,
                                                    y,
                                                    (void **) &theModule,
                                                    &theLen) ==
           QTSS_NoErr; y++) {
    Assert(theModule != nullptr);
    Assert(theLen == sizeof(QTSSModule *));

    (*theModule)->GetPrefsDict()->RereadPreferences();

#if DEBUG
    theModule = nullptr;
    theLen = 0;
#endif
  }

  //
  // Go through each module's prefs object and have those reread as well

  //
  // Now that we are done rereading the prefs, invoke all modules in the RereadPrefs
  // role so they can update their internal prefs caches.
  for (UInt32 x = 0;
       x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRereadPrefsRole);
       x++) {
    QTSSModule *theModule =
        QTSServerInterface::GetModule(QTSSModule::kRereadPrefsRole, x);
    (void) theModule->CallDispatch(QTSS_RereadPrefs_Role, nullptr);
  }
  return QTSS_NoErr;
}


