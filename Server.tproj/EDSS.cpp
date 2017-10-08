//
// Created by james on 10/5/17.
//

#include "EDSS.h"

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

    inner = new EDSS();


    return inner;
  }

  return nullptr;
}

EDSS::EDSS() : theServerState(qtssStartingUpState) {

}

EDSS~EDSSConfigure() {

}

//
// Life cycle check points

CF_Error EDSS::AfterInitBase() {

}

CF_Error EDSS::AfterConfigThreads(UInt32 numThreads) {

}

CF_Error EDSS::AfterConfigFramework() {

}

CF_Error EDSS::StartupCustomServices() {

}

//
// User Settings

char *EDSS::GetPersonalityUser() {

}

char *EDSS::GetPersonalityGroup() {

}

//
// TaskThreadPool Settings

UInt32 EDSS::GetShortTaskThreads() {

}

UInt32 EDSS::GetBlockingThreads() {

}
