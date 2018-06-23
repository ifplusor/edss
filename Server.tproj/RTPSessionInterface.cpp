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
    File:       RTPSessionInterface.h

    Contains:   Implementation of object defined in .h
*/

#include <CF/md5.h>
#include <CF/md5digest.h>
#include <CF/base64.h>
#include <CF/Core/Time.h>

#include "RTPSessionInterface.h"
#include "RTSPRequestInterface.h"
#include "RTPStream.h"

std::atomic_uint RTPSessionInterface::sRTPSessionIDCounter{0};

QTSSAttrInfoDict::AttrInfo  RTPSessionInterface::sAttributes[] =
    {   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
        /* 0  */{"qtssCliSesStreamObjects", NULL, qtssAttrDataTypeQTSS_Object, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 1  */{"qtssCliSesCreateTimeInMsec", NULL, qtssAttrDataTypeTimeVal, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 2  */{"qtssCliSesFirstPlayTimeInMsec", NULL, qtssAttrDataTypeTimeVal, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 3  */{"qtssCliSesPlayTimeInMsec", NULL, qtssAttrDataTypeTimeVal, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 4  */{"qtssCliSesAdjustedPlayTimeInMsec", NULL, qtssAttrDataTypeTimeVal, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 5  */{"qtssCliSesRTPBytesSent", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 6  */{"qtssCliSesRTPPacketsSent", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 7  */{"qtssCliSesState", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 8  */{"qtssCliSesPresentationURL", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 9  */{"qtssCliSesFirstUserAgent", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 10 */{"qtssCliStrMovieDurationInSecs", NULL, qtssAttrDataTypeFloat64, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite},
        /* 11 */{"qtssCliStrMovieSizeInBytes", NULL, qtssAttrDataTypeUInt64, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite},
        /* 12 */{"qtssCliSesMovieAverageBitRate", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite},
        /* 13 */{"qtssCliSesLastRTSPSession", NULL, qtssAttrDataTypeQTSS_Object, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 14 */{"qtssCliSesFullURL", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 15 */{"qtssCliSesHostName", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},

        /* 16 */{"qtssCliRTSPSessRemoteAddrStr", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 17 */{"qtssCliRTSPSessLocalDNS", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 18 */{"qtssCliRTSPSessLocalAddrStr", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 19 */{"qtssCliRTSPSesUserName", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 20 */{"qtssCliRTSPSesUserPassword", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 21 */{"qtssCliRTSPSesURLRealm", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 22 */{"qtssCliRTSPReqRealStatusCode", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 23 */{"qtssCliTeardownReason", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite},
        /* 24 */{"qtssCliSesReqQueryString", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 25 */{"qtssCliRTSPReqRespMsg", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},

        /* 26 */{"qtssCliSesCurrentBitRate", CurrentBitRate, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 27 */{"qtssCliSesPacketLossPercent", PacketLossPercent, qtssAttrDataTypeFloat32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 28 */{"qtssCliSesTimeConnectedinMsec", TimeConnected, qtssAttrDataTypeSInt64, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 29 */{"qtssCliSesCounterID", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 30 */{"qtssCliSesRTSPSessionID", NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 31 */{"qtssCliSesFramesSkipped", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe},
        /* 32 */{"qtssCliSesTimeoutMsec", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe},
        /* 33 */{"qtssCliSesOverBufferEnabled", NULL, qtssAttrDataTypeBool16, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe},
        /* 34 */{"qtssCliSesRTCPPacketsRecv", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 35 */{"qtssCliSesRTCPBytesRecv", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe},
        /* 36 */{"qtssCliSesStartedThinning", NULL, qtssAttrDataTypeBool16, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe},
        /* 37 */{"qtssCliSessLastRTSPBandwidth", NULL, qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe}
    };

void RTPSessionInterface::Initialize() {
  for (UInt32 x = 0; x < qtssCliSesNumParams; x++)
    QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex)
        ->SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr, sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

RTPSessionInterface::RTPSessionInterface()
    : QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex), NULL),
      Task(),
      fLastQualityCheckTime(0),
      fLastQualityCheckMediaTime(0),
      fStartedThinning(false),
      fIsFirstPlay(true),
      fAllTracksInterleaved(true), // assume true until proven false!
      fFirstPlayTime(0),
      fPlayTime(0),
      fAdjustedPlayTime(0),
      fNTPPlayTime(0),
      fNextSendPacketsTime(0),
      fSessionQualityLevel(0),
      fState(qtssPausedState),
      fPlayFlags(0),
      fLastBitRateBytes(0),
      fLastBitRateUpdateTime(0),
      fMovieCurrentBitRate(0),
      fRTSPSession(NULL),
      fLastRTSPReqRealStatusCode(200),
      fTimeoutTask(NULL, QTSServerInterface::GetServer()->GetPrefs()->GetRTPSessionTimeoutInSecs() * 1000),
      fNumQualityLevels(0),
      fBytesSent(0),
      fPacketsSent(0),
      fPacketLossPercent(0.0),
      fTimeConnected(0),
      fTotalRTCPPacketsRecv(0),
      fTotalRTCPBytesRecv(0),
      fMovieDuration(0),
      fMovieSizeInBytes(0),
      fMovieAverageBitRate(0),
      fTeardownReason(0),
      fUniqueID(0),
      fTracker(QTSServerInterface::GetServer()->GetPrefs()->IsSlowStartEnabled()),
      fOverbufferWindow(QTSServerInterface::GetServer()->GetPrefs()->GetSendIntervalInMsec(), kUInt32_Max,
                        QTSServerInterface::GetServer()->GetPrefs()->GetMaxSendAheadTimeInSecs(),
                        QTSServerInterface::GetServer()->GetPrefs()->GetOverbufferRate()),
      fAuthScheme(QTSServerInterface::GetServer()->GetPrefs()->GetAuthScheme()),
      fAuthQop(RTSPSessionInterface::kNoQop),
      fAuthNonceCount(0),
      fFramesSkipped(0),
      fLastRTSPBandwidthHeaderBits(0) {
  //don't actually setup the fTimeoutTask until the session has been bound!
  //(we don't want to get timeouts before the session gets bound)

  fTimeoutTask.SetTask(this);
  fTimeout = QTSServerInterface::GetServer()->GetPrefs()->GetRTPSessionTimeoutInSecs() * 1000;
  //fUniqueID = (UInt32)atomic_add(&sRTPSessionIDCounter, 1);
  fUniqueID = ++sRTPSessionIDCounter;

  // fQualityUpdate is a counter the starting value is the unique ID so every session starts at a different position
  fQualityUpdate = fUniqueID;

  //mark the session create time
  fSessionCreateTime = CF::Core::Time::Milliseconds();

  // Setup all dictionary attribute values

  // Make sure the dictionary knows about our preallocated memory for the RTP stream array
  this->SetEmptyVal(qtssCliSesFirstUserAgent, &fUserAgentBuffer[0], kUserAgentBufSize);
  this->SetEmptyVal(qtssCliSesStreamObjects, &fStreamBuffer[0], kStreamBufSize);
  this->SetEmptyVal(qtssCliSesPresentationURL, &fPresentationURL[0], kPresentationURLSize);
  this->SetEmptyVal(qtssCliSesFullURL, &fFullRequestURL[0], kRequestHostNameBufferSize);
  this->SetEmptyVal(qtssCliSesHostName, &fRequestHostName[0], kFullRequestURLBufferSize);

  this->SetVal(qtssCliSesCreateTimeInMsec, &fSessionCreateTime, sizeof(fSessionCreateTime));
  this->SetVal(qtssCliSesFirstPlayTimeInMsec, &fFirstPlayTime, sizeof(fFirstPlayTime));
  this->SetVal(qtssCliSesPlayTimeInMsec, &fPlayTime, sizeof(fPlayTime));
  this->SetVal(qtssCliSesAdjustedPlayTimeInMsec, &fAdjustedPlayTime, sizeof(fAdjustedPlayTime));
  this->SetVal(qtssCliSesRTPBytesSent, &fBytesSent, sizeof(fBytesSent));
  this->SetVal(qtssCliSesRTPPacketsSent, &fPacketsSent, sizeof(fPacketsSent));
  this->SetVal(qtssCliSesState, &fState, sizeof(fState));
  this->SetVal(qtssCliSesMovieDurationInSecs, &fMovieDuration, sizeof(fMovieDuration));
  this->SetVal(qtssCliSesMovieSizeInBytes, &fMovieSizeInBytes, sizeof(fMovieSizeInBytes));
  this->SetVal(qtssCliSesLastRTSPSession, &fRTSPSession, sizeof(fRTSPSession));
  this->SetVal(qtssCliSesMovieAverageBitRate, &fMovieAverageBitRate, sizeof(fMovieAverageBitRate));
  this->SetEmptyVal(qtssCliRTSPSessRemoteAddrStr, &fRTSPSessRemoteAddrStr[0], kIPAddrStrBufSize);
  this->SetEmptyVal(qtssCliRTSPSessLocalDNS, &fRTSPSessLocalDNS[0], kLocalDNSBufSize);
  this->SetEmptyVal(qtssCliRTSPSessLocalAddrStr, &fRTSPSessLocalAddrStr[0], kIPAddrStrBufSize);

  this->SetEmptyVal(qtssCliRTSPSesUserName, &fUserNameBuf[0], RTSPSessionInterface::kMaxUserNameLen);
  this->SetEmptyVal(qtssCliRTSPSesUserPassword, &fUserPasswordBuf[0], RTSPSessionInterface::kMaxUserPasswordLen);
  this->SetEmptyVal(qtssCliRTSPSesURLRealm, &fUserRealmBuf[0], RTSPSessionInterface::kMaxUserRealmLen);

  this->SetVal(qtssCliRTSPReqRealStatusCode, &fLastRTSPReqRealStatusCode, sizeof(fLastRTSPReqRealStatusCode));

  this->SetVal(qtssCliTeardownReason, &fTeardownReason, sizeof(fTeardownReason));
  //   this->SetVal(qtssCliSesCurrentBitRate, &fMovieCurrentBitRate, sizeof(fMovieCurrentBitRate));
  this->SetVal(qtssCliSesCounterID, &fUniqueID, sizeof(fUniqueID));
  this->SetEmptyVal(qtssCliSesRTSPSessionID, &fRTSPSessionIDBuf[0], QTSS_MAX_SESSION_ID_LENGTH + 4);
  this->SetVal(qtssCliSesFramesSkipped, &fFramesSkipped, sizeof(fFramesSkipped));
  this->SetVal(qtssCliSesRTCPPacketsRecv, &fTotalRTCPPacketsRecv, sizeof(fTotalRTCPPacketsRecv));
  this->SetVal(qtssCliSesRTCPBytesRecv, &fTotalRTCPBytesRecv, sizeof(fTotalRTCPBytesRecv));

  this->SetVal(qtssCliSesTimeoutMsec, &fTimeout, sizeof(fTimeout));

  this->SetVal(qtssCliSesOverBufferEnabled, this->GetOverbufferWindow()->OverbufferingEnabledPtr(), sizeof(bool));
  this->SetVal(qtssCliSesStartedThinning, &fStartedThinning, sizeof(bool));

  this->SetVal(qtssCliSessLastRTSPBandwidth, &fLastRTSPBandwidthHeaderBits, sizeof(fLastRTSPBandwidthHeaderBits));
}

void RTPSessionInterface::SetValueComplete(UInt32 inAttrIndex,
                                           QTSSDictionaryMap *inMap,
                                           UInt32 inValueIndex,
                                           void *inNewValue,
                                           UInt32 inNewValueLen) {
  if (inAttrIndex == qtssCliSesTimeoutMsec) {
    Assert(inNewValueLen == sizeof(UInt32));
    UInt32 newTimeOut = *((UInt32 *) inNewValue);
    fTimeoutTask.SetTimeout((SInt64) newTimeOut);
  }
}

void RTPSessionInterface::UpdateRTSPSession(RTSPSessionInterface *inNewRTSPSession) {
  if (inNewRTSPSession != fRTSPSession) {
    // If there was an old session, let it know that we are done
    if (fRTSPSession != NULL)
      fRTSPSession->DecrementObjectHolderCount();

    // Increment this count to prevent the RTSP session from being deleted
    fRTSPSession = inNewRTSPSession;
    fRTSPSession->IncrementObjectHolderCount();
  }
}

char *RTPSessionInterface::GetSRBuffer(UInt32 inSRLen) {
  if (fSRBuffer.Len < inSRLen) {
    delete[] fSRBuffer.Ptr;
    fSRBuffer.Set(new char[2 * inSRLen], 2 * inSRLen);
  }
  return fSRBuffer.Ptr;
}

QTSS_Error RTPSessionInterface::DoSessionSetupResponse(RTSPRequestInterface *inRequest) {
  // This function appends a session header to the SETUP response, and
  // checks to see if it is a 304 Not Modified. If it is, it sends the entire
  // response and returns an error
  if (QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutInSecs() > 0)  // adv the timeout
    inRequest->AppendSessionHeaderWithTimeout(this->GetValue(qtssCliSesRTSPSessionID),
                                              QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutAsString());
  else
    inRequest->AppendSessionHeaderWithTimeout(this->GetValue(qtssCliSesRTSPSessionID), NULL); // no timeout in resp.

  if (inRequest->GetStatus() == qtssRedirectNotModified) {
    (void) inRequest->SendHeader();
    return QTSS_RequestFailed;
  }
  return QTSS_NoErr;
}

void RTPSessionInterface::UpdateBitRateInternal(const SInt64 &curTime) {
  if (fState == qtssPausedState) {
    fMovieCurrentBitRate = 0;
    fLastBitRateUpdateTime = curTime;
    fLastBitRateBytes = fBytesSent;
  } else {
    UInt32 bitsInInterval = (fBytesSent - fLastBitRateBytes) * 8;
    SInt64 updateTime = (curTime - fLastBitRateUpdateTime) / 1000;
    if (updateTime > 0) // leave Bit Rate the same if updateTime is 0 also don't divide by 0.
      fMovieCurrentBitRate = (UInt32) (bitsInInterval / updateTime);
    fTracker.UpdateAckTimeout(bitsInInterval, curTime - fLastBitRateUpdateTime);
    fLastBitRateBytes = fBytesSent;
    fLastBitRateUpdateTime = curTime;
  }
  //s_printf("fMovieCurrentBitRate=%"   _U32BITARG_   "\n",fMovieCurrentBitRate);
  //s_printf("Cur bandwidth: %d. Cur ack timeout: %d.\n",fTracker.GetCurrentBandwidthInBps(), fTracker.RecommendedClientAckTimeout());
}

void *RTPSessionInterface::TimeConnected(QTSSDictionary *inSession,
                                         UInt32 *outLen) {
  RTPSessionInterface *theSession = (RTPSessionInterface *) inSession;
  theSession->fTimeConnected =
      (Core::Time::Milliseconds() - theSession->GetSessionCreateTime());

  // Return the result
  *outLen = sizeof(theSession->fTimeConnected);
  return &theSession->fTimeConnected;
}

void *RTPSessionInterface::CurrentBitRate(QTSSDictionary *inSession,
                                          UInt32 *outLen) {
  RTPSessionInterface *theSession = (RTPSessionInterface *) inSession;
  theSession->UpdateBitRateInternal(Core::Time::Milliseconds());

  // Return the result
  *outLen = sizeof(theSession->fMovieCurrentBitRate);
  return &theSession->fMovieCurrentBitRate;
}

void *RTPSessionInterface::PacketLossPercent(QTSSDictionary *inSession,
                                             UInt32 *outLen) {
  RTPSessionInterface *theSession = (RTPSessionInterface *) inSession;
  RTPStream *theStream = NULL;
  UInt32 theLen = sizeof(theStream);

  SInt64 packetsLost = 0;
  SInt64 packetsSent = 0;

  for (int x = 0; theSession->GetValue(qtssCliSesStreamObjects,
                                       x,
                                       (void *) &theStream,
                                       &theLen) == QTSS_NoErr; x++) {
    if (theStream != NULL) {
      UInt32 streamCurPacketsLost = 0;
      theLen = sizeof(UInt32);
      (void) theStream->GetValue(qtssRTPStrCurPacketsLostInRTCPInterval,
                                 0,
                                 &streamCurPacketsLost,
                                 &theLen);
      //s_printf("stream = %d streamCurPacketsLost = %"   _U32BITARG_   " \n",x, streamCurPacketsLost);

      UInt32 streamCurPackets = 0;
      theLen = sizeof(UInt32);
      (void) theStream->GetValue(qtssRTPStrPacketCountInRTCPInterval,
                                 0,
                                 &streamCurPackets,
                                 &theLen);
      //s_printf("stream = %d streamCurPackets = %"   _U32BITARG_   " \n",x, streamCurPackets);

      packetsSent += (SInt64) streamCurPackets;
      packetsLost += (SInt64) streamCurPacketsLost;
      //s_printf("stream calculated loss = %f \n",x, (Float32) streamCurPacketsLost / (Float32) streamCurPackets);

    }

    theStream = NULL;
    theLen = sizeof(UInt32);
  }

  //Assert(packetsLost <= packetsSent);
  if (packetsSent > 0) {
    if (packetsLost <= packetsSent)
      theSession->fPacketLossPercent =
          (Float32) ((((Float32) packetsLost / (Float32) packetsSent) * 100.0));
    else
      theSession->fPacketLossPercent = 100.0;
  } else
    theSession->fPacketLossPercent = 0.0;
  //s_printf("Session loss percent packetsLost = %qd packetsSent= %qd theSession->fPacketLossPercent=%f\n",packetsLost,packetsSent,theSession->fPacketLossPercent);
  // Return the result
  *outLen = sizeof(theSession->fPacketLossPercent);
  return &theSession->fPacketLossPercent;
}

void RTPSessionInterface::CreateDigestAuthenticationNonce() {

  // Calculate nonce: MD5 of sessionid:timestamp
  SInt64 curTime = Core::Time::Milliseconds();
  char *curTimeStr = new char[128];
  s_sprintf(curTimeStr, "%" _64BITARG_ "d", curTime);

  // Delete old nonce before creating a new one
  if (fAuthNonce.Ptr != NULL)
    delete[] fAuthNonce.Ptr;

  MD5_CTX ctxt;
  unsigned char nonceStr[16];
  unsigned char colon[] = ":";
  MD5_Init(&ctxt);
  StrPtrLen *sesID = this->GetValue(qtssCliSesRTSPSessionID);
  MD5_Update(&ctxt, (unsigned char *) sesID->Ptr, sesID->Len);
  MD5_Update(&ctxt, (unsigned char *) colon, 1);
  MD5_Update(&ctxt, (unsigned char *) curTimeStr, ::strlen(curTimeStr));
  MD5_Final(nonceStr, &ctxt);
  HashToString(nonceStr, &fAuthNonce);

  delete[] curTimeStr; // No longer required once nonce is created

  // Set the nonce count value to zero
  // as a new nonce has been created
  fAuthNonceCount = 0;

}

void RTPSessionInterface::SetChallengeParams(QTSS_AuthScheme scheme,
                                             UInt32 qop,
                                             bool newNonce,
                                             bool createOpaque) {
  // Set challenge params
  // Set authentication scheme
  fAuthScheme = scheme;

  if (fAuthScheme == qtssAuthDigest) {
    // Set Quality of Protection
    // auth-int (Authentication with integrity) not supported yet
    fAuthQop = qop;

    if (newNonce || (fAuthNonce.Ptr == NULL))
      this->CreateDigestAuthenticationNonce();

    if (createOpaque) {
      // Generate a random UInt32 and convert it to a string
      // The base64 encoded form of the string is made the opaque value
      SInt64 theMicroseconds = Core::Time::Microseconds();
      ::srand((unsigned int) theMicroseconds);
      UInt32 randomNum = ::rand();
      char *randomNumStr = new char[128];
      s_sprintf(randomNumStr, "%"   _U32BITARG_   "", randomNum);
      int len = ::strlen(randomNumStr);
      fAuthOpaque.Len = Base64encode_len(len);
      char *opaqueStr = new char[fAuthOpaque.Len];
      (void) Base64encode(opaqueStr, randomNumStr, len);
      delete[] randomNumStr;                 // Don't need this anymore
      if (fAuthOpaque.Ptr
          != NULL)             // Delete existing pointer before assigning new
        delete[] fAuthOpaque.Ptr;              // one
      fAuthOpaque.Ptr = opaqueStr;
    } else {
      if (fAuthOpaque.Ptr != NULL)
        delete[] fAuthOpaque.Ptr;
      fAuthOpaque.Len = 0;
    }
    // Increase the Nonce Count by one
    // This number is a count of the next request the server
    // expects with this nonce. (Implies that the server
    // has already received nonce count - 1 requests that
    // sent authorization with this nonce
    fAuthNonceCount++;
  }
}

void RTPSessionInterface::UpdateDigestAuthChallengeParams(bool newNonce,
                                                          bool createOpaque,
                                                          UInt32 qop) {
  if (newNonce || (fAuthNonce.Ptr == NULL))
    this->CreateDigestAuthenticationNonce();

  if (createOpaque) {
    // Generate a random UInt32 and convert it to a string
    // The base64 encoded form of the string is made the opaque value
    SInt64 theMicroseconds = Core::Time::Microseconds();
    ::srand((unsigned int) theMicroseconds);
    UInt32 randomNum = ::rand();
    char *randomNumStr = new char[128];
    s_sprintf(randomNumStr, "%"   _U32BITARG_   "", randomNum);
    int len = ::strlen(randomNumStr);
    fAuthOpaque.Len = Base64encode_len(len);
    char *opaqueStr = new char[fAuthOpaque.Len];
    (void) Base64encode(opaqueStr, randomNumStr, len);
    delete[] randomNumStr;                 // Don't need this anymore
    if (fAuthOpaque.Ptr
        != NULL)             // Delete existing pointer before assigning new
      delete[] fAuthOpaque.Ptr;              // one
    fAuthOpaque.Ptr = opaqueStr;
    fAuthOpaque.Len = ::strlen(opaqueStr);
  } else {
    if (fAuthOpaque.Ptr != NULL)
      delete[] fAuthOpaque.Ptr;
    fAuthOpaque.Len = 0;
  }
  fAuthNonceCount++;

  fAuthQop = qop;
}
