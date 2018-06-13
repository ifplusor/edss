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
    File:       win32main.cpp
    Contains:   main function to drive streaming server on win32.
*/

#include "getopt.h"
#include "FilePrefsSource.h"
#include "RunServer.h"
#include "QTSServer.h"
#include "GenerateXMLPrefs.h"

#include "EDSS.h"

// Data
static FilePrefsSource sPrefsSource(true); // Allow dups
static XMLPrefsParser *sXMLParser = NULL;
static FilePrefsSource sMessagesSource;
static UInt16 sPort = 0; //port can be set on the command line
static int sStatsUpdateInterval = 0;
static SERVICE_STATUS_HANDLE sServiceStatusHandle = 0;
static QTSS_ServerState sInitialState = qtssRunningState;

// Functions
static void ReportStatus(DWORD inCurrentState, DWORD inExitCode);
static void InstallService(char *inServiceName);
static void RemoveService(char *inServiceName);
static void RunAsService(char *inServiceName);
void WINAPI ServiceControl(DWORD);
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);



CF_Error CFInit(int argc, char **argv) {
  extern char *optarg;

  char absolutePath[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, absolutePath);

  char *theConfigFilePath = "config.cfg";
  char *theXMLFilePath = "config.xml";
  bool notAService = false;
  bool theXMLPrefsExist = true;
  bool dontFork = false;

#if _DEBUG
  char* compileType = "Compile_Flags/_DEBUG; ";
#else
  char *compileType = "Compile_Flags/_RELEASE;";
#endif

  s_printf("%s/%s ( Build/%s; Platform/%s; %s%s) Built on: %s\n",
           QTSServerInterface::GetServerName().Ptr,
           QTSServerInterface::GetServerVersion().Ptr,
           QTSServerInterface::GetServerBuild().Ptr,
           QTSServerInterface::GetServerPlatform().Ptr,
           compileType,
           QTSServerInterface::GetServerComment().Ptr,
           QTSServerInterface::GetServerBuildDate().Ptr);

  //First thing to do is to read command-line arguments.
  int ch;
  while ((ch = getopt(argc, argv, "vdxp:o:c:irsS:I")) != EOF) { // opt: means requires option
    switch (ch) {
      case 'v':
        s_printf("%s/%s ( Build/%s; Platform/%s; %s%s) Built on: %s\n",
                 QTSServerInterface::GetServerName().Ptr,
                 QTSServerInterface::GetServerVersion().Ptr,
                 QTSServerInterface::GetServerBuild().Ptr,
                 QTSServerInterface::GetServerPlatform().Ptr,
                 compileType,
                 QTSServerInterface::GetServerComment().Ptr,
                 QTSServerInterface::GetServerBuildDate().Ptr);
        s_printf("usage: %s [ -d | -p port | -v | -c /myconfigpath.xml | -o /myconfigpath.conf | -x | -S numseconds | -I | -h ]\n", QTSServerInterface::GetServerName().Ptr);
        s_printf("-d: Don't run as a Win32 Service\n");
        s_printf("-p XXX: Specify the default RTSP listening port of the server\n");
        s_printf("-c c:\\myconfigpath.xml: Specify a config file path\n");
        s_printf("-o c:\\myconfigpath.conf: Specify a DSS 1.x / 2.x config file path\n");
        s_printf("-x: Force create new .xml config file from 1.x / 2.x config\n");
        s_printf("-i: Install the EasyDarwin service\n");
        s_printf("-r: Remove the EasyDarwin service\n");
        s_printf("-s: Start the EasyDarwin service\n");
        s_printf("-S n: Display server stats in the console every \"n\" seconds\n");
        s_printf("-I: Start the server in the idle state\n");
        ::exit(0);
      case 'd':
        notAService = true;
        break;
      case 'p':
        Assert(optarg != NULL);// this means we didn't declare getopt options correctly or there is a bug in getopt.
        sPort = (UInt16) ::atoi(optarg);
        break;
      case 'c':
        Assert(optarg != NULL);// this means we didn't declare getopt options correctly or there is a bug in getopt.
        theXMLFilePath = optarg;
        break;
      case 'S':
        Assert(optarg != NULL);// this means we didn't declare getopt options correctly or there is a bug in getopt.
        sStatsUpdateInterval = ::atoi(optarg);
        break;
      case 'o':
        Assert(optarg != NULL);// this means we didn't declare getopt options correctly or there is a bug in getopt.
        theConfigFilePath = optarg;
        break;
      case 'x':
        theXMLPrefsExist = false; // Force us to generate a new XML prefs file
        break;
      case 'i':
        s_printf("Installing the EDSS service...\n");
        ::InstallService("EDSS");
        s_printf("Starting the EDSS service...\n");
        ::RunAsService("EDSS");
        ::exit(0);
      case 'r':
        s_printf("Removing the EDSS service...\n");
        ::RemoveService("EDSS");
        ::exit(0);
      case 's':
        s_printf("Starting the EDSS service...\n");
        ::RunAsService("EDSS");
        ::exit(0);
      case 'I':
        sInitialState = qtssIdleState;
        break;
      default: break;
    }
  }

  //
  // Create an XML prefs parser object using the specified path
  sXMLParser = new XMLPrefsParser(theXMLFilePath);

  //
  // Check to see if the XML file exists as a directory. If it does,
  // just bail because we do not want to overwrite a directory
  if (sXMLParser->DoesFileExistAsDirectory()) {
    s_printf("Directory located at location where streaming server prefs file should be.\n");
    ::exit(0);
  }

  if (!sXMLParser->CanWriteFile()) {
    s_printf("Cannot write to the streaming server prefs file.\n");
    ::exit(0);
  }

  // If we aren't forced to create a new XML prefs file, whether
  // we do or not depends solely on whether the XML prefs file exists currently.
  if (theXMLPrefsExist)
    theXMLPrefsExist = sXMLParser->DoesFileExist();

  if (!theXMLPrefsExist) {
    //
    //Construct a Prefs Source object to get server preferences
    int prefsErr = sPrefsSource.InitFromConfigFile(theConfigFilePath);
    if (prefsErr)
      s_printf("Could not load configuration file at %s.\n Generating a new prefs file at %s\n", theConfigFilePath, theXMLFilePath);

    //
    // Generate a brand-new XML prefs file out of the old prefs
    int xmlGenerateErr = GenerateAllXMLPrefs(&sPrefsSource, sXMLParser);
    if (xmlGenerateErr) {
      s_printf("Fatal Error: Could not create new prefs file at: %s. (%d)\n", theConfigFilePath, CF::Core::Thread::GetErrno());
      ::exit(-1);
    }
  }

  //
  // Parse the configs from the XML file
  int xmlParseErr = sXMLParser->Parse();
  if (xmlParseErr) {
    s_printf("Fatal Error: Could not load configuration file at %s. (%d)\n", theXMLFilePath, CF::Core::Thread::GetErrno());
    ::exit(-1);
  }

  //
  // Construct a messages source object
  sMessagesSource.InitFromConfigFile("qtssmessages.txt");

  EDSS *edss = EDSS::StartServer(sXMLParser, &sMessagesSource,
                                 sPort,
                                 sStatsUpdateInterval,
                                 sInitialState,
                                 false,
                                 0, kRunServerDebug_Off,
                                 absolutePath);
  CF::CFEnv::Register(edss);
  return CF_NoErr;


  if (notAService) {
    // If we're running off the command-line, don't do the service initiation crap.
    ::StartServer(sXMLParser, &sMessagesSource, sPort, sStatsUpdateInterval, sInitialState, false, 0, kRunServerDebug_Off, absolutePath); // No stats update interval for now
    ::RunServer();
    ::exit(0);
  }

  SERVICE_TABLE_ENTRY dispatchTable[] = {
      {"", ServiceMain},
      {NULL, NULL}
  };

  //
  // In case someone runs the server improperly, print out a friendly message.
  s_printf("edss2 must either be started from the DOS Console\n");
  s_printf("using the -d command-line option, or using the Service Control Manager\n\n");
  s_printf("Waiting for the Service Control Manager to start edss...\n");
  BOOL theErr = ::StartServiceCtrlDispatcher(dispatchTable);
  if (!theErr) {
    s_printf("Fatal Error: Couldn't start Service\n");
    ::exit(-1);
  }

  return CF_NoErr;
}

CF_Error CFExit(CF_Error exitCode) {
  return CF_NoErr;
}


#if 0
bool GetWindowsServiceInstallPath(char* inPath)
{
    HKEY hKEY;
    if (RegOpenKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EasyDarwin", &hKEY) == ERROR_SUCCESS)
    {
        DWORD dwType;
        DWORD cchData;
        dwType = REG_EXPAND_SZ;
        char szName[MAX_PATH];
        cchData = sizeof(szName);
        if (RegQueryValueEx(hKEY, "ImagePath", NULL, &dwType, (LPBYTE)szName, &cchData) == ERROR_SUCCESS)
        {
            char* p = strstr(szName, "EasyDarwin.exe");
            if (p != NULL)
            {
                int len = p - szName;
                memcpy(inPath, szName, p - szName);
                inPath[len] = '\0';
                return 1;
            }
        }
        RegCloseKey(hKEY);
    }
    return 0;
}
#endif
void __stdcall ServiceMain(DWORD /*argc*/, LPTSTR *argv) {
  char *theServerName = argv[0];
  char sAbsolutePath[MAX_PATH];
  ::GetModuleFileName(NULL, sAbsolutePath, MAX_PATH);
  int cnt = strlen(sAbsolutePath);
  for (int i = cnt; i >= 0; --i) {
    if (sAbsolutePath[i] == '\\') {
      sAbsolutePath[i + 1] = '\0';
      break;
    }
  }
  sServiceStatusHandle = ::RegisterServiceCtrlHandler(theServerName, &ServiceControl);
  if (sServiceStatusHandle == 0) {
    s_printf("Failure registering service handler");
    return;
  }

  //
  // Report our status
  ::ReportStatus(SERVICE_START_PENDING, NO_ERROR);


  //
  // Start & Run the server - no stats update interval for now
  if (::StartServer(sXMLParser, &sMessagesSource, sPort, sStatsUpdateInterval, sInitialState, false, 0, kRunServerDebug_Off, sAbsolutePath) != qtssFatalErrorState) {
    ::ReportStatus(SERVICE_RUNNING, NO_ERROR);
    ::RunServer(); // This function won't return until the server has died

    //
    // Ok, server is done...
    ::ReportStatus(SERVICE_STOPPED, NO_ERROR);
  } else
    ::ReportStatus(SERVICE_STOPPED, ERROR_BAD_COMMAND); // I dunno... report some error

#ifdef WIN32
  ::ExitProcess(0);
#else
  ::exit(0);
#endif //WIN32
}

void WINAPI ServiceControl(DWORD inControlCode) {
  QTSS_ServerState theState;
  QTSServerInterface *theServer = QTSServerInterface::GetServer();
  DWORD theStatusReport = SERVICE_START_PENDING;

  if (theServer != NULL)
    theState = theServer->GetServerState();
  else
    theState = qtssStartingUpState;

  switch (inControlCode) {
    // Stop the service.
    //
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN: {
      if (theState == qtssStartingUpState)
        break;

      //
      // Signal the server to shut down.
      theState = qtssShuttingDownState;
      if (theServer != NULL)
        theServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
      break;
    }
    case SERVICE_CONTROL_PAUSE: {
      if (theState != qtssRunningState)
        break;

      //
      // Signal the server to refuse new connections.
      theState = qtssRefusingConnectionsState;
      if (theServer != NULL)
        theServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
      break;
    }
    case SERVICE_CONTROL_CONTINUE: {
      if (theState != qtssRefusingConnectionsState)
        break;

      //
      // Signal the server to refuse new connections.
      theState = qtssRefusingConnectionsState;
      if (theServer != NULL)
        theServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
      break;
    }
    case SERVICE_CONTROL_INTERROGATE: break; // Just update our status

    default: break;
  }

  if (theServer != NULL) {
    theState = theServer->GetServerState();

    //
    // Convert a QTSS state to a Win32 Service state
    switch (theState) {
      case qtssStartingUpState: theStatusReport = SERVICE_START_PENDING;
        break;
      case qtssRunningState: theStatusReport = SERVICE_RUNNING;
        break;
      case qtssRefusingConnectionsState: theStatusReport = SERVICE_PAUSED;
        break;
      case qtssFatalErrorState: theStatusReport = SERVICE_STOP_PENDING;
        break;
      case qtssShuttingDownState: theStatusReport = SERVICE_STOP_PENDING;
        break;
      default: theStatusReport = SERVICE_RUNNING;
        break;
    }
  } else
    theStatusReport = SERVICE_START_PENDING;

  s_printf("Reporting status from ServiceControl function\n");
  ::ReportStatus(theStatusReport, NO_ERROR);

}

void ReportStatus(DWORD inCurrentState, DWORD inExitCode) {
  static bool sFirstTime = 1;
  static UInt32 sCheckpoint = 0;
  static SERVICE_STATUS sStatus;

  if (sFirstTime) {
    sFirstTime = false;

    //
    // Setup the status structure
    sStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    sStatus.dwCurrentState = SERVICE_START_PENDING;
    //sStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN;
    sStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    sStatus.dwWin32ExitCode = 0;
    sStatus.dwServiceSpecificExitCode = 0;
    sStatus.dwCheckPoint = 0;
    sStatus.dwWaitHint = 0;
  }

  if (sStatus.dwCurrentState == SERVICE_START_PENDING)
    sStatus.dwCheckPoint = ++sCheckpoint;
  else
    sStatus.dwCheckPoint = 0;

  sStatus.dwCurrentState = inCurrentState;
  sStatus.dwServiceSpecificExitCode = inExitCode;
  BOOL theErr = SetServiceStatus(sServiceStatusHandle, &sStatus);
  if (theErr == 0) {
    DWORD theerrvalue = ::GetLastError();
  }
}

void RunAsService(char *inServiceName) {
  SC_HANDLE theService;
  SC_HANDLE theSCManager;

  theSCManager = ::OpenSCManager(
      NULL,                   // machine (NULL == local)
      NULL,                   // database (NULL == default)
      SC_MANAGER_ALL_ACCESS   // access required
  );
  if (!theSCManager)
    return;

  theService = ::OpenService(
      theSCManager,               // SCManager database
      inServiceName,               // name of service
      SERVICE_ALL_ACCESS);

  SERVICE_STATUS lpServiceStatus;

  if (theService) {
    const SInt32 kNotRunning = 1062;
    bool stopped = ::ControlService(theService, SERVICE_CONTROL_STOP, &lpServiceStatus);
    if (!stopped && ((SInt32) ::GetLastError() != kNotRunning))
      s_printf("Stopping Service Error: %d\n", ::GetLastError());

    bool started = ::StartService(theService, 0, NULL);
    if (!started)
      s_printf("Starting Service Error: %d\n", ::GetLastError());

    ::CloseServiceHandle(theService);
  }

  ::CloseServiceHandle(theSCManager);
}

void InstallService(char *inServiceName) {
  SC_HANDLE theService;
  SC_HANDLE theSCManager;

  TCHAR thePath[512];
  TCHAR theQuotedPath[522];

  BOOL theErr = ::GetModuleFileName(NULL, thePath, 512);
  if (!theErr)
    return;

  s_sprintf(theQuotedPath, "\"%s\"", thePath);

  theSCManager = ::OpenSCManager(
      NULL,                   // machine (NULL == local)
      NULL,                   // database (NULL == default)
      SC_MANAGER_ALL_ACCESS   // access required
  );
  if (!theSCManager) {
    s_printf("Failed to install EasyDarwin Service\n");
    return;
  }

  theService = CreateService(
      theSCManager,               // SCManager database
      inServiceName,               // name of service
      inServiceName,               // name to display
      SERVICE_ALL_ACCESS,         // desired access
      SERVICE_WIN32_OWN_PROCESS,  // service type
      SERVICE_AUTO_START,       // start type
      SERVICE_ERROR_NORMAL,       // error control type
      theQuotedPath,               // service's binary
      NULL,                       // no load ordering group
      NULL,                       // no tag identifier
      NULL,       // dependencies
      NULL,                       // LocalSystem account
      NULL);                      // no password

  if (theService) {
    ::CloseServiceHandle(theService);
    s_printf("Installed EasyDarwin Service\n");
  } else
    s_printf("Failed to install EasyDarwin Service\n");

  ::CloseServiceHandle(theSCManager);
}

void RemoveService(char *inServiceName) {
  SC_HANDLE theSCManager;
  SC_HANDLE theService;

  theSCManager = ::OpenSCManager(
      NULL,                   // machine (NULL == local)
      NULL,                   // database (NULL == default)
      SC_MANAGER_ALL_ACCESS   // access required
  );
  if (!theSCManager) {
    s_printf("Failed to remove EasyDarwin Service\n");
    return;
  }

  theService = ::OpenService(theSCManager, inServiceName, SERVICE_ALL_ACCESS);
  if (theService != NULL) {
    bool stopped = ::ControlService(theService, SERVICE_CONTROL_STOP, NULL);
    if (!stopped)
      s_printf("Stopping Service Error: %d\n", ::GetLastError());

    (void) ::DeleteService(theService);
    ::CloseServiceHandle(theService);
    s_printf("Removed EasyDarwin Service\n");
  } else
    s_printf("Failed to remove EasyDarwin Service\n");

  ::CloseServiceHandle(theSCManager);
}
