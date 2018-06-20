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
    File:       QTSServer.h

    Contains:   This object is responsible for bringing up & shutting down
                the server. It also loads & initializes all modules.



*/

#ifndef __QTSSERVER_H__
#define __QTSSERVER_H__

#include "QTSServerInterface.h"

class RTCPTask;

class RTSPListenerSocket;

class RTPSocketPool;

/**
 * 属于 Server core 一部分。处理 server 的 startup 和 shutdown。
 */
class QTSServer : public QTSServerInterface {
 public:

  QTSServer() = default;
  ~QTSServer() override;

  //
  // Initialize
  //
  // This function starts the server. If it returns true, the server has
  // started up sucessfully. If it returns false, a fatal error occurred
  // while attempting to start the server.
  //
  // This function *must* be called before the server creates any threads,
  // because one of its actions is to change the server to the right UID / GID.
  // Threads will only inherit these if they are created afterwards.
  bool Initialize(XMLPrefsParser *inPrefsSource, PrefsSource *inMessagesSource, UInt16 inPortOverride,
                  bool createListeners, char const *inAbsolutePath);

  //
  // InitModules
  //
  // Initialize *does not* do much of the module initialization tasks. This
  // function may be called after the server has created threads, but the
  // server must not be in a state where it can do real work. In other words,
  // call this function right after calling Initialize.
  void InitModules(QTSS_ServerState inEndState);

  //
  // StartTasks
  //
  // The server has certain global tasks that it runs for things like stats
  // updating and RTCP processing. This function must be called to start those
  // going, and it must be called after Initialize
  void StartTasks();


  //
  // RereadPrefsService
  //
  // This service is registered by the server (calling "RereadPreferences").
  // It rereads the preferences. Anyone can call this to reread the preferences,
  // and it may be called safely at any time, though it will fail with a
  // QTSS_OutOfState if the server isn't in the qtssRunningState.

  static QTSS_Error RereadPrefsService(QTSS_ServiceFunctionArgsPtr inArgs);

  //
  // CreateListeners
  //
  // This function may be called multiple times & at any time.
  // It updates the server's listeners to reflect what the preferences say.
  // Returns false if server couldn't listen on one or more of the ports, true otherwise
  bool CreateListeners(bool startListeningNow, QTSServerPrefs *inPrefs, UInt16 inPortOverride);

  //
  // SetDefaultIPAddr
  //
  // Sets the IP address related attributes of the server.
  bool SetDefaultIPAddr();

  bool SetupUDPSockets();

  bool SwitchPersonality();

  char sAbsolutePath[MAX_PATH]{};
 private:

  //
  // GLOBAL TASKS
  RTCPTask *fRTCPTask{};
  RTPStatsUpdaterTask *fStatsTask{};
  static char const *sPortPrefString;
  static XMLPrefsParser *sPrefsSource;
  static PrefsSource *sMessagesSource;

  //
  // Module loading & unloading routines

  static QTSS_Callbacks sCallbacks;

  // Sets up QTSS API callback routines
  void InitCallbacks();

  // Loads compiled-in modules
  void LoadCompiledInModules();

  // Loads modules from disk
  void LoadModules(QTSServerPrefs *inPrefs);

  void CreateModule(char *inModuleFolderPath, char *inModuleName);

  // Adds a module to the module array
  bool AddModule(QTSSModule *inModule);

  void PostRegisterModules();

  // Call module init roles
  void DoInitRole();

  void SetupPublicHeader();

  UInt32 *GetRTSPIPAddrs(QTSServerPrefs *inPrefs, UInt32 *outNumAddrsPtr);

  UInt16 *GetRTSPPorts(QTSServerPrefs *inPrefs, UInt32 *outNumPortsPtr);

  // Build & destroy the optimized role / module arrays for invoking modules
  void BuildModuleRoleArrays();

  void DestroyModuleRoleArrays();

#ifndef __Win32__
  static pid_t sMainPid;
#endif

  friend class RTPSocketPool;
};

class RereadPrefsTask : public CF::Thread::Task {
 public:
  SInt64 Run() override {
    QTSServer::RereadPrefsService(nullptr);
    return -1;
  }
};

#endif // __QTSSERVER_H__


