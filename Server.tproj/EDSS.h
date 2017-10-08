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

class EDSS : public CF::CFConfigure {
 public:
  static EDSS *StartServer(XMLPrefsParser *inPrefsSource,
                           PrefsSource *inMessagesSource,
                           UInt16 inPortOverride,
                           int statsUpdateInterval,
                           QTSS_ServerState inInitialState,
                           bool inDontFork,
                           UInt32 debugLevel,
                           UInt32 debugOptions,
                           const char *sAbsolutePath);

  EDSS();
  ~EDSS() override;

  //
  // Life cycle check points

  CF_Error AfterInitBase() override;
  CF_Error AfterConfigThreads(UInt32 numThreads) override;
  CF_Error AfterConfigFramework() override;
  CF_Error StartupCustomServices() override;

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

 private:
  QTSS_ServerState theServerState;

  static QTSServer *sServer;
  static int sStatusUpdateInterval;
  static bool sHasPID;
  static UInt64 sLastStatusPackets;
  static UInt64 sLastDebugPackets;
  static SInt64 sLastDebugTotalQuality;
};

#endif //__EDSS2_EDSSCONFIG_H__
