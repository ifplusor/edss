//
// Created by james on 10/5/17.
//

#include "EDSS.h"
#include <CF/ArrayObjectDeleter.h>
#include <QTSSRollingLog.h>
#include <QTSSModuleUtils.h>

#if __Linux__
#include <sys/stat.h>
#endif

enum {
  kRunServerDebug_Off = 0,
  kRunServerDebugDisplay_On = 1 << 0,
  kRunServerDebugLogging_On = 1 << 1 // not implemented
};

inline bool DebugOn(QTSServer *server) {
  return server->GetDebugOptions() != kRunServerDebug_Off;
}
inline bool DebugDisplayOn(QTSServer *server) {
  return (server->GetDebugOptions() & kRunServerDebugDisplay_On) != 0;
}
inline bool DebugLogOn(QTSServer *server) {
  return (server->GetDebugOptions() & kRunServerDebugLogging_On) != 0;
}

void WritePid(bool forked);
void CleanPid(bool force);


using namespace CF;

QTSServer *EDSS::sServer = NULL;
int EDSS::sStatusUpdateInterval = 0;
bool EDSS::sHasPID = false;
UInt64 EDSS::sLastStatusPackets = 0;
UInt64 EDSS::sLastDebugPackets = 0;
SInt64 EDSS::sLastDebugTotalQuality = 0;


EDSS *EDSS::StartServer(XMLPrefsParser *inPrefsSource,
                        PrefsSource *inMessagesSource,
                        UInt16 inPortOverride,
                        int statsUpdateInterval,
                        QTSS_ServerState inInitialState,
                        bool inDontFork,
                        UInt32 debugLevel,
                        UInt32 debugOptions,
                        const char *sAbsolutePath) {
  static EDSS *inner = nullptr;
  if (inner == nullptr) {
    sStatusUpdateInterval = statsUpdateInterval;

    inner = new EDSS(inPrefsSource,
                     inMessagesSource,
                     inPortOverride,
                     inInitialState,
                     inDontFork,
                     debugLevel,
                     debugOptions,
                     sAbsolutePath);

    return inner;
  }

  return nullptr;
}

EDSS::EDSS(XMLPrefsParser *inPrefsSource,
           PrefsSource *inMessagesSource,
           UInt16 inPortOverride,
           QTSS_ServerState inInitialState,
           bool inDontFork,
           UInt32 debugLevel,
           UInt32 debugOptions,
           const char *inAbsolutePath)
    : prefsSource(inPrefsSource),
      messagesSource(inMessagesSource),
      portOverride(inPortOverride),
      initialState(inInitialState),
      dontFork(inDontFork),
      debugLevel(debugLevel),
      debugOptions(debugOptions) {
  sAbsolutePath = strdup(inAbsolutePath);
  loopCount = 0;
}

EDSS::~EDSS() {
  free(sAbsolutePath);
}

//
// Life cycle check points

CF_Error EDSS::AfterInitBase() {
  // start the server
  QTSSDictionaryMap::Initialize();
  QTSServerInterface::Initialize(); // this must be called before constructing the server object
  sServer = new QTSServer();
  sServer->SetDebugLevel(debugLevel);
  sServer->SetDebugOptions(debugOptions);

  // re-parse config file
  prefsSource->Parse();

  bool createListeners = qtssShuttingDownState != initialState;

  sServer->Initialize(prefsSource, messagesSource, portOverride, createListeners, sAbsolutePath);

  if (qtssShuttingDownState == initialState) {
    sServer->InitModules(initialState);
    return CF_ShuttingDown;
  }

  if (qtssFatalErrorState == sServer->GetServerState())
    return CF_FatalError;

  return CF_NoErr;
}

CF_Error EDSS::AfterConfigThreads(UInt32 numThreads) {
  sServer->InitNumThreads(numThreads);
  return CF_NoErr;
}

CF_Error EDSS::AfterConfigFramework() {
  //
  // On Win32, in order to call modwatch, the Socket EventQueue thread must be
  // created first. Modules call modwatch from their initializer, and we don't
  // want to prevent them from doing that, so module initialization is separated
  // out from other initialization, and we start the Socket EventQueue thread first.
  // The server is still prevented from doing anything as of yet, because there
  // aren't any TaskThreads yet.
  sServer->InitModules(initialState);
  return CF_NoErr;
}

CF_Error EDSS::StartupCustomServices() {
  sServer->StartTasks();
  sServer->SetupUDPSockets(); // udp sockets are set up after the rtcp task is instantiated

  if (qtssFatalErrorState != sServer->GetServerState()) {
    CleanPid(true);
    WritePid(!dontFork);

    s_printf("Streaming Server done starting up\n");
    return CF_NoErr;
  }

  return CF_FatalError;
}

CF_Error EDSS::DoIdle() {
  QTSS_ServerState theServerState = sServer->GetServerState();
  if ((theServerState != qtssShuttingDownState) &&
      (theServerState != qtssFatalErrorState)) {
    LogStatus(theServerState);

    if (sStatusUpdateInterval) {
      debugLevel = sServer->GetDebugLevel();
      bool printHeader = PrintHeader(loopCount);
      bool printStatus = PrintLine(loopCount);

      if (printStatus) {
        if (DebugOn(sServer)) // debug level display or logging is on
          DebugStatus(debugLevel, printHeader);

        if (!DebugDisplayOn(sServer))
          PrintStatus(printHeader); // default status output
      }

      loopCount++;
    }

    if ((sServer->SigIntSet()) || (sServer->SigTermSet())) {
      //
      // start the shutdown process
      theServerState = qtssShuttingDownState;
      (void) QTSS_SetValue(QTSServerInterface::GetServer(),
                           qtssSvrState,
                           0,
                           &theServerState,
                           sizeof(theServerState));

      if (sServer->SigIntSet())
        bool restartServer = true;
    }

    theServerState = sServer->GetServerState();
    if (theServerState == qtssIdleState)
      sServer->KillAllRTPSessions();
  } else {
    CFEnv::Exit(CF_ShuttingDown);
  }

  return CF_NoErr;
}

//
// User Settings

char *EDSS::GetPersonalityUser() {
  static char userName[256] = {0};
  CF::CharArrayDeleter runUserName(sServer->GetPrefs()->GetRunUserName());
  strncpy(userName, runUserName.GetObject(), 255);
  return userName;
}

char *EDSS::GetPersonalityGroup() {
  static char groupName[256] = {0};
  CF::CharArrayDeleter runGroupName(sServer->GetPrefs()->GetRunGroupName());
  strncpy(groupName, runGroupName.GetObject(), 255);
  return groupName;
}

//
// TaskThreadPool Settings

UInt32 EDSS::GetShortTaskThreads() {
  return sServer->GetPrefs()->GetNumThreads(); // whatever the prefs say
}

UInt32 EDSS::GetBlockingThreads() {
  return sServer->GetPrefs()->GetNumBlockingThreads(); // whatever the prefs say
}


//
// ===========================

void WritePid(bool forked);
//void WritePid(bool forked) {
//#if !__Win32__ && !__MinGW__
//  // WRITE PID TO FILE
//  CF::CharArrayDeleter thePidFileName(sServer->GetPrefs()->GetPidFilePath());
//  FILE *thePidFile = fopen(thePidFileName, "w");
//  if (thePidFile) {
//    if (!forked) {
//      fprintf(thePidFile, "%d\n", getpid());    // write own pid
//    } else {
//      fprintf(thePidFile, "%d\n", getppid());    // write parent pid
//      fprintf(thePidFile, "%d\n", getpid());    // and our own pid in the next line
//    }
//    fclose(thePidFile);
//    sHasPID = true;
//  }
//#endif
//}

void CleanPid(bool force);
//void CleanPid(bool force) {
//#if !__Win32__ && !__MinGW__
//  if (sHasPID || force) {
//    CF::CharArrayDeleter thePidFileName(sServer->GetPrefs()->GetPidFilePath());
//    unlink(thePidFileName);
//  }
//#endif
//}


void EDSS::LogStatus(QTSS_ServerState theServerState) {
  static QTSS_ServerState lastServerState = 0;
  static char *sPLISTHeader[] = {
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
#if __MacOSX__
      "<!DOCTYPE plist SYSTEM \"file://localhost/System/Library/DTDs/PropertyList.dtd\">",
#else
      "<!ENTITY % plistObject \"(array | data | date | dict | real | integer | string | true | false )\">",
      "<!ELEMENT plist %plistObject;>",
      "<!ATTLIST plist version CDATA \"0.9\">",
      "",
      "<!-- Collections -->",
      "<!ELEMENT array (%plistObject;)*>",
      "<!ELEMENT dict (key, %plistObject;)*>",
      "<!ELEMENT key (#PCDATA)>",
      "",
      "<!--- Primitive types -->",
      "<!ELEMENT string (#PCDATA)>",
      "<!ELEMENT data (#PCDATA)> <!-- Contents interpreted as Base-64 encoded -->",
      "<!ELEMENT date (#PCDATA)> <!-- Contents should conform to a subset of ISO 8601 (in particular, YYYY '-' MM '-' DD 'T' HH ':' MM ':' SS 'Z'.  Smaller units may be omitted with a loss of precision) -->",
      "",
      "<!-- Numerical primitives -->",
      "<!ELEMENT true EMPTY>  <!-- Boolean constant true -->",
      "<!ELEMENT false EMPTY> <!-- Boolean constant false -->",
      "<!ELEMENT real (#PCDATA)> <!-- Contents should represent a floating point number matching (\"+\" | \"-\")? d+ (\".\"d*)? (\"E\" (\"+\" | \"-\") d+)? where d is a digit 0-9.  -->",
      "<!ELEMENT integer (#PCDATA)> <!-- Contents should represent a (possibly signed) integer number in base 10 -->",
      "]>",
#endif
  };

  static int numHeaderLines = sizeof(sPLISTHeader) / sizeof(char *);

  static char *sPlistStart = "<plist version=\"0.9\">";
  static char *sPlistEnd = "</plist>";
  static char *sDictStart = "<dict>";
  static char *sDictEnd = "</dict>";

  static char *sKey = "     <key>%s</key>\n";
  static char *sValue = "     <string>%s</string>\n";

  static char *sAttributes[] = {
      "qtssSvrServerName",
      "qtssSvrServerVersion",
      "qtssSvrServerBuild",
      "qtssSvrServerPlatform",
      "qtssSvrRTSPServerComment",
      "qtssSvrServerBuildDate",
      "qtssSvrStartupTime",
      "qtssSvrCurrentTimeMilliseconds",
      "qtssSvrCPULoadPercent",
      "qtssSvrState",
      "qtssRTPSvrCurConn",
      "qtssRTSPCurrentSessionCount",
      "qtssRTSPHTTPCurrentSessionCount",
      "qtssRTPSvrCurBandwidth",
      "qtssRTPSvrCurPackets",
      "qtssRTPSvrTotalConn",
      "qtssRTPSvrTotalBytes"
  };
  static int numAttributes = sizeof(sAttributes) / sizeof(char *);

  static CF::StrPtrLen statsFileNameStr("server_status");

  if (!sServer->GetPrefs()->ServerStatFileEnabled())
    return;

  UInt32 interval = sServer->GetPrefs()->GetStatFileIntervalSec();
  if (interval == 0 || (CF::Core::Time::UnixTime_Secs() % interval) > 0)
    return;

  // If the total number of RTSP sessions is 0  then we
  // might not need to update the "server_status" file.
  char *thePrefStr = NULL;
  // We start lastRTSPSessionCount off with an impossible value so that
  // we force the "server_status" file to be written at least once.
  static int lastRTSPSessionCount = -1;
  // Get the RTSP session count from the server.
  (void) QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
  int currentRTSPSessionCount = ::atoi(thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;
  if (currentRTSPSessionCount == 0
      && currentRTSPSessionCount == lastRTSPSessionCount) {
    // we don't need to update the "server_status" file except the
    // first time we are in the idle state.
    if (theServerState == qtssIdleState && lastServerState == qtssIdleState) {
      lastRTSPSessionCount = currentRTSPSessionCount;
      lastServerState = theServerState;
      return;
    }
  } else {
    // save the RTSP session count for the next time we execute.
    lastRTSPSessionCount = currentRTSPSessionCount;
  }

  CF::StrPtrLenDel pathStr(sServer->GetPrefs()->GetErrorLogDir());
  CF::StrPtrLenDel fileNameStr(sServer->GetPrefs()->GetStatsMonitorFileName());
  CF::ResizeableStringFormatter pathBuffer(NULL, 0);
  pathBuffer.PutFilePath(&pathStr, &fileNameStr);
  pathBuffer.PutTerminator();

  char *filePath = pathBuffer.GetBufPtr();
  FILE *statusFile = ::fopen(filePath, "w");
  char *theAttributeValue = NULL;
  int i;

  if (statusFile != NULL) {
    ::chmod(filePath, 0640);
    for (i = 0; i < numHeaderLines; i++) {
      s_fprintf(statusFile, "%s\n", sPLISTHeader[i]);
    }

    s_fprintf(statusFile, "%s\n", sPlistStart);
    s_fprintf(statusFile, "%s\n", sDictStart);

    // show each element value
    for (i = 0; i < numAttributes; i++) {
      (void) QTSS_GetValueAsString(sServer, QTSSModuleUtils::GetAttrID(sServer, sAttributes[i]), 0, &theAttributeValue);
      if (theAttributeValue != NULL) {
        s_fprintf(statusFile, sKey, sAttributes[i]);
        s_fprintf(statusFile, sValue, theAttributeValue);
        delete[] theAttributeValue;
        theAttributeValue = NULL;
      }
    }

    s_fprintf(statusFile, "%s\n", sDictEnd);
    s_fprintf(statusFile, "%s\n\n", sPlistEnd);

    ::fclose(statusFile);
  }
  lastServerState = theServerState;
}

bool EDSS::PrintHeader(UInt32 loopCount) {
  return (loopCount % (sStatusUpdateInterval * 10)) == 0;
}

bool EDSS::PrintLine(UInt32 loopCount) {
  return (loopCount % sStatusUpdateInterval) == 0;
}

void FormattedTotalBytesBuffer(char *outBuffer, int outBufferLen, UInt64 totalBytes);
//void FormattedTotalBytesBuffer(char *outBuffer, int outBufferLen, UInt64 totalBytes) {
//  Float32 displayBytes = 0.0;
//  char sizeStr[] = "B";
//  char *format = NULL;
//
//  if (totalBytes > 1073741824) { //GBytes
//    displayBytes = (Float32) ((Float64) (SInt64) totalBytes / (Float64) (SInt64) 1073741824);
//    sizeStr[0] = 'G';
//    format = "%.4f%s ";
//  } else if (totalBytes > 1048576) { //MBytes
//    displayBytes = (Float32) (SInt32) totalBytes / (Float32) (SInt32) 1048576;
//    sizeStr[0] = 'M';
//    format = "%.3f%s ";
//  } else if (totalBytes > 1024) { //KBytes
//    displayBytes = (Float32) (SInt32) totalBytes / (Float32) (SInt32) 1024;
//    sizeStr[0] = 'K';
//    format = "%.2f%s ";
//  } else {
//    displayBytes = (Float32) (SInt32) totalBytes;  //Bytes
//    sizeStr[0] = 'B';
//    format = "%4.0f%s ";
//  }
//
//  outBuffer[outBufferLen - 1] = 0;
//  s_snprintf(outBuffer, outBufferLen - 1, format, displayBytes, sizeStr);
//}

void EDSS::PrintStatus(bool printHeader) {
  char *thePrefStr = NULL;
  UInt32 theLen = 0;

  if (printHeader) {
    s_printf(
        "     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec    TotConn     TotBytes   TotPktsLost          Time\n");
  }

  (void) QTSS_GetValueAsString(sServer, qtssRTPSvrCurConn, 0, &thePrefStr);
  s_printf("%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  (void) QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
  s_printf("%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  (void) QTSS_GetValueAsString(sServer, qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
  s_printf("%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  UInt32 curBandwidth = 0;
  theLen = sizeof(curBandwidth);
  (void) QTSS_GetValue(sServer, qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
  s_printf("%11" _U32BITARG_, curBandwidth / 1024);

  (void) QTSS_GetValueAsString(sServer, qtssRTPSvrCurPackets, 0, &thePrefStr);
  s_printf("%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  (void) QTSS_GetValueAsString(sServer, qtssRTPSvrTotalConn, 0, &thePrefStr);
  s_printf("%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  UInt64 totalBytes = sServer->GetTotalRTPBytes();
  char displayBuff[32] = "";
  FormattedTotalBytesBuffer(displayBuff, sizeof(displayBuff), totalBytes);
  s_printf("%17s", displayBuff);

  s_printf("%11" _64BITARG_ "u", sServer->GetTotalRTPPacketsLost());

  char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
  (void) QTSSRollingLog::FormatDate(theDateBuffer, false);
  s_printf("%25s", theDateBuffer);

  s_printf("\n");
}

void print_status(FILE *file, FILE *console, char *format, char *theStr);
//void print_status(FILE *file, FILE *console, char *format, char *theStr) {
//  if (file) s_fprintf(file, format, theStr);
//  if (console) s_fprintf(console, format, theStr);
//}

void EDSS::DebugLevel_1(FILE *statusFile, FILE *stdOut, bool printHeader) {
  char *thePrefStr = NULL;
  static char numStr[12] = "";
  static char dateStr[25] = "";
  UInt32 theLen = 0;

  if (printHeader) {
    print_status(statusFile, stdOut, "%s",
                 "     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned      Time\n");

  }

  (void) QTSS_GetValueAsString(sServer, qtssRTPSvrCurConn, 0, &thePrefStr);
  print_status(statusFile, stdOut, "%11s", thePrefStr);

  delete[] thePrefStr;
  thePrefStr = NULL;

  (void) QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
  print_status(statusFile, stdOut, "%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  (void) QTSS_GetValueAsString(sServer, qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
  print_status(statusFile, stdOut, "%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  UInt32 curBandwidth = 0;
  theLen = sizeof(curBandwidth);
  (void) QTSS_GetValue(sServer, qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
  s_snprintf(numStr, 11, "%"   _U32BITARG_   "", curBandwidth / 1024);
  print_status(statusFile, stdOut, "%11s", numStr);

  (void) QTSS_GetValueAsString(sServer, qtssRTPSvrCurPackets, 0, &thePrefStr);
  print_status(statusFile, stdOut, "%11s", thePrefStr);
  delete[] thePrefStr;
  thePrefStr = NULL;

  UInt32 currentPlaying = sServer->GetNumRTPPlayingSessions();
  s_snprintf(numStr, sizeof(numStr) - 1, "%"   _U32BITARG_   "", currentPlaying);
  print_status(statusFile, stdOut, "%14s", numStr);


  //is the server keeping up with the streams?
  //what quality are the streams?
  SInt64 totalRTPPaackets = sServer->GetTotalRTPPackets();
  SInt64 deltaPackets = totalRTPPaackets - sLastDebugPackets;
  sLastDebugPackets = totalRTPPaackets;

  SInt64 totalQuality = sServer->GetTotalQuality();
  SInt64 deltaQuality = totalQuality - sLastDebugTotalQuality;
  sLastDebugTotalQuality = totalQuality;

  SInt64 currentMaxLate = sServer->GetCurrentMaxLate();
  SInt64 totalLate = sServer->GetTotalLate();

  sServer->ClearTotalLate();
  sServer->ClearCurrentMaxLate();
  sServer->ClearTotalQuality();

  s_snprintf(numStr, sizeof(numStr) - 1, "%s", "0");
  if (deltaPackets > 0)
    s_snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (SInt32) ((SInt64) totalLate / (SInt64) deltaPackets));
  print_status(statusFile, stdOut, "%11s", numStr);

  s_snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (SInt32) currentMaxLate);
  print_status(statusFile, stdOut, "%11s", numStr);

  s_snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (SInt32) sServer->GetMaxLate());
  print_status(statusFile, stdOut, "%11s", numStr);

  s_snprintf(numStr, sizeof(numStr) - 1, "%s", "0");
  if (deltaPackets > 0) {
    s_snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (SInt32) ((SInt64) deltaQuality / (SInt64) deltaPackets));
  }
  print_status(statusFile, stdOut, "%11s", numStr);

  s_snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (SInt32) sServer->GetNumThinned());
  print_status(statusFile, stdOut, "%11s", numStr);

  char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
  (void) QTSSRollingLog::FormatDate(theDateBuffer, false);

  s_snprintf(dateStr, sizeof(dateStr) - 1, "%s", theDateBuffer);
  print_status(statusFile, stdOut, "%24s\n", dateStr);
}

FILE *EDSS::LogDebugEnabled() {

  if (DebugLogOn(sServer)) {
    static CF::StrPtrLen statsFileNameStr("server_debug_status");

    CF::StrPtrLenDel pathStr(sServer->GetPrefs()->GetErrorLogDir());
    CF::ResizeableStringFormatter pathBuffer(NULL, 0);
    pathBuffer.PutFilePath(&pathStr, &statsFileNameStr);
    pathBuffer.PutTerminator();

    char *filePath = pathBuffer.GetBufPtr();
    return ::fopen(filePath, "a");
  }

  return NULL;
}

FILE *EDSS::DisplayDebugEnabled() {
  return (DebugDisplayOn(sServer)) ? stdout : NULL;
}

void EDSS::DebugStatus(UInt32 debugLevel, bool printHeader) {

  FILE *statusFile = LogDebugEnabled();
  FILE *stdOut = DisplayDebugEnabled();

  if (debugLevel > 0)
    DebugLevel_1(statusFile, stdOut, printHeader);

  if (statusFile)
    ::fclose(statusFile);
}
