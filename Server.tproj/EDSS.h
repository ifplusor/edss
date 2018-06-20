//
// Created by james on 10/5/17.
//

#ifndef __EDSS2_EDSSCONFIG_H__
#define __EDSS2_EDSSCONFIG_H__

#include <CF/CF.h>

#include <XMLPrefsParser.h>
#include <PrefsSource.h>
#include <QTSS.h>
#include <QTSServer.h>


enum {
  kRunServerDebug_Off = 0x00,
  kRunServerDebugDisplay_On = 0x01U << 0U,
  kRunServerDebugLogging_On = 0x01U << 1U // not implemented
};


class EDSS : public CF::CFConfigure {
 public:

  /**
   * 配置工厂方法
   */
  static EDSS *StartServer(XMLPrefsParser *inPrefsSource, PrefsSource *inMessagesSource, UInt16 inPortOverride,
                           int statsUpdateInterval, QTSS_ServerState inInitialState, bool inDontFork,
                           UInt32 debugLevel, UInt32 debugOptions, char const *sAbsolutePath);

  ~EDSS() override;

  //
  // Life cycle check points

  CF_Error AfterInitBase() override;
  CF_Error AfterConfigThreads(UInt32 numThreads) override;
  CF_Error AfterConfigFramework() override;
  CF_Error StartupCustomServices() override;
  CF_Error DoIdle() override;

  //
  // User Settings

  char *GetPersonalityUser() override;
  char *GetPersonalityGroup() override;

  //
  // TaskThreadPool Settings

  UInt32 GetShortTaskThreads() override;
  UInt32 GetBlockingThreads() override;

  //
  // Http Server Settings

  //
  // Utils

  static void WritePid(bool forked);
  static void CleanPid(bool force);

 private:

  EDSS(XMLPrefsParser *inPrefsSource, PrefsSource *inMessagesSource, UInt16 inPortOverride,
       QTSS_ServerState inInitialState, bool inDontFork, UInt32 debugLevel, UInt32 debugOptions,
       char const *sAbsolutePath);

  //
  // Member fuctions

  void LogStatus(QTSS_ServerState theServerState);
  bool PrintHeader(UInt32 loopCount);
  bool PrintLine(UInt32 loopCount);
  void PrintStatus(bool printHeader);
  void DebugLevel_1(FILE *statusFile, FILE *stdOut, bool printHeader);
  FILE *LogDebugEnabled();
  FILE *DisplayDebugEnabled();
  void DebugStatus(UInt32 debugLevel, bool printHeader);

 private:
  XMLPrefsParser *prefsSource;
  PrefsSource *messagesSource;
  UInt16 portOverride;
  QTSS_ServerState initialState;
  bool dontFork;
  UInt32 debugLevel;
  UInt32 debugOptions;
  char *sAbsolutePath;

  UInt32 loopCount;

  // global member
  static QTSServer *sServer;
  static int sStatusUpdateInterval;
  static bool sHasPID;
  static UInt64 sLastStatusPackets;
  static UInt64 sLastDebugPackets;
  static SInt64 sLastDebugTotalQuality;
};

#endif //__EDSS2_EDSSCONFIG_H__
