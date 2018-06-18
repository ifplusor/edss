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
    File:       RTSPSession.h

    Contains:   Represents an RTSP session (duh), which I define as a complete TCP connection
                lifetime, from connection to FIN or RESET termination. This object is
                the active element that gets scheduled and gets work done. It creates requests
                and processes them when data arrives. When it is time to close the connection
                it takes care of that.
*/

#ifndef __RTSPSESSION_H__
#define __RTSPSESSION_H__

#include "RTSPSessionInterface.h"
#include "RTSPRequestStream.h"
#include "RTSPRequest.h"
#include "RTPSession.h"

//class RTSPMsg;
//class RTSPSessionHandler;
class RTSPSession;

//class RTSPMsg
//{
//public:
//	RTSPMsg() : fQueueElem() { fQueueElem.SetEnclosingObject(this); this->Reset(); }
//	void Reset() { // make packet ready to reuse fQueueElem is always in use
//		fTimeArrived = 0;
//		fPacketPtr.Set(fPacketData, 0);
//		fIsData = true;
//		fMsgCountID = 0;
//	}
//
//	~RTSPMsg() {}
//
//	void    SetMsgData(char *data, UInt32 len)
//	{
//		Assert(kMaxRTSPMsgLen > len);
//
//		if (len > kMaxRTSPMsgLen)
//        {
//            printf("len > kMaxRTSPMsgLen\n");
//			len = kMaxRTSPMsgLen;
//        }
//
//		if (len > 0)
//			memcpy(this->fPacketPtr.Ptr, data, len);
//		this->fPacketPtr.Len = len;
//	}
//
//	bool  IsData() { return fIsData; }
//	inline  UInt16  GetPacketSeqNum();
//
//private:
//
//	enum
//	{
//		kMaxRTSPMsgLen = 2060
//	};
//
//	SInt64      fTimeArrived;
//	OSQueueElem fQueueElem;
//	char        fPacketData[kMaxRTSPMsgLen];
//	StrPtrLen   fPacketPtr;
//	bool      fIsData;
//	UInt64      fMsgCountID;
//
//	friend class RTSPSession;
//	friend class RTSPSessionHandler;
//};
//
//UInt16 RTSPMsg::GetPacketSeqNum()
//{
//	return 0;
//}

class RTSPSession : public RTSPSessionInterface {
 public:

  explicit RTSPSession(bool doReportHTTPConnectionAddress);

  ~RTSPSession() override;

  // Call this before using this object
  static void Initialize();

  static void PostRegisterModules();

  bool IsPlaying() {
    if (fRTPSession == NULL) return false;
    return fRTPSession->GetSessionState() == qtssPlayingState;
  }

 private:

  SInt64 Run();

  // Gets & creates RTP session for this request.
  QTSS_Error FindRTPSession(RefTable *inTable);

  QTSS_Error CreateNewRTPSession(RefTable *inTable);

  void SetupClientSessionAttrs();

  // Does request prep & request cleanup, respectively
  void SetupRequest();

  void CleanupRequest();

  bool ParseOptionsResponse();

  // Fancy random number generator
  UInt32 GenerateNewSessionID(char *ioBuffer);

  // Sends an error response & returns error if not ok.
  QTSS_Error IsOkToAddNewRTPSession();

  // Checks authentication parameters
  void CheckAuthentication();

  // test current connections handled by this object against server pref connection limit
  bool OverMaxConnections(UInt32 buffer);

  char fLastRTPSessionID[QTSS_MAX_SESSION_ID_LENGTH];
  StrPtrLen fLastRTPSessionIDPtr;

  RTSPRequest *fRequest;
  RTPSession *fRTPSession;

  //RTSPSessionHandler* fRTSPSessionHandler;


  /* -- begin adds for HTTP ProxyTunnel -- */

  // This gets grabbed whenever the input side of the session is being used.
  // It is to protect POST snarfage while input stuff is in action
  Core::Mutex fReadMutex;

  Ref *RegisterRTSPSessionIntoHTTPProxyTunnelMap(QTSS_RTSPSessionType inSessionType);

  QTSS_Error PreFilterForHTTPProxyTunnel();              // prefilter for HTTP proxies
  bool ParseProxyTunnelHTTP();                     // use by PreFilterForHTTPProxyTunnel
  void HandleIncomingDataPacket();

  static RefTable *sHTTPProxyTunnelMap;    // a map of available partners.

  enum {
    kMaxHTTPResponseLen = 512
  };
  static char sHTTPResponseHeaderBuf[kMaxHTTPResponseLen];
  static StrPtrLen sHTTPResponseHeaderPtr;

  static char sHTTPResponseNoServerHeaderBuf[kMaxHTTPResponseLen];
  static StrPtrLen sHTTPResponseNoServerHeaderPtr;

  static const char *sHTTPResponseFormatStr;
  static const char *sHTTPNoServerResponseFormatStr;
  char fProxySessionID[QTSS_MAX_SESSION_ID_LENGTH
  ];    // our magic cookie to match proxy connections
  StrPtrLen fProxySessionIDPtr;
  Ref fProxyRef;
  enum {
    // the kinds of HTTP Methods we're interested in for
    // RTSP tunneling
        kHTTPMethodInit       // initialize to this
    , kHTTPMethodUnknown    // tested, but unknown
    , kHTTPMethodGet        // found one of these methods...
    , kHTTPMethodPost
  };

  UInt16 fHTTPMethod;
  bool fWasHTTPRequest;
  bool fFoundValidAccept;
  bool fDoReportHTTPConnectionAddress; // true if we need to report our IP adress in reponse to the clients GET request (necessary for servers behind DNS round robin)
  /* -- end adds for HTTP ProxyTunnel -- */


  // Module invocation and module state.
  // This info keeps track of our current state so that
  // the state machine works properly.
  // RTSPSession的状态机:
  enum {
    kReadingRequest = 0,
    kFilteringRequest = 1,
    kRoutingRequest = 2,
    kAuthenticatingRequest = 3,
    kAuthorizingRequest = 4,
    kPreprocessingRequest = 5,
    kProcessingRequest = 6,
    kSendingResponse = 7,
    kPostProcessingRequest = 8,
    kCleaningUp = 9,

    // states that RTSP sessions that setup RTSP
    // through HTTP tunnels pass through
    kWaitingToBindHTTPTunnel = 10,              // POST or GET side waiting to be joined with it's matching half
    kSocketHasBeenBoundIntoHTTPTunnel = 11,     // POST side after attachment by GET side ( its dying )
    kHTTPFilteringRequest = 12,                 // after kReadingRequest, enter this state
    kReadingFirstRequest = 13,                  // initial state - the only time we look for an HTTP tunnel
    kHaveNonTunnelMessage = 14                  // we've looked at the message, and its not an HTTP tunnle message
  };

  UInt32 fCurrentModule;
  UInt32 fState;

  QTSS_RoleParams fRoleParams;  // module param blocks for roles.
  QTSS_ModuleState fModuleState;

  QTSS_Error SetupAuthLocalPath(RTSPRequest *theRTSPRequest);

  void SaveRequestAuthorizationParams(RTSPRequest *theRTSPRequest);

  QTSS_Error DumpRequestData();

  UInt64 fMsgCount;

  //friend class RTSPSessionHandler;

};

//class RTSPSessionHandler : public Task
//{
//public:
//	RTSPSessionHandler(RTSPSession* session);
//	virtual ~RTSPSessionHandler();
//
//private:
//	virtual SInt64 Run();
//    
//    void Release();
//    RTSPSession* fRTSPSession;
//
//    //Number of packets to allocate when the socket is first created
//	enum
//	{
//		kNumPreallocatedMsgs = 20,   //UInt32
//        sMsgHandleInterval = 1
//	};
//
//	OSQueue fFreeMsgQueue;
//	OSQueue fMsgQueue;
//    OSMutex fQueueMutex;
//    OSMutex fFreeQueueMutex;
//
//    bool  fLiveHandler;
//
//public:
//    RTSPMsg* GetMsg();
//	bool  ProcessMsg(const SInt64& inMilliseconds, RTSPMsg* theMsg);
//    void HandleDataPacket(RTSPMsg* msg);
//
//    friend class RTSPSession;
//};

#endif // __RTSPSESSION_H__

