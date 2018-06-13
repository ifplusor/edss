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
    Copyright (c) 2013-2016 EasyDarwin.ORG.  All rights reserved.
    Github: https://github.com/EasyDarwin
    WEChat: EasyDarwin
    Website: http://www.easydarwin.org
*/
/*
    File:       QTSSAdminModule.cpp
    Contains:   Implements Admin module
*/

#include <string.h>

#ifdef _WIN32
#include <direct.h>
#endif // _WIN32

#ifndef __Win32__
#endif

#include <stdlib.h>     /* for getloadavg & other useful stuff */
#include "QTSSAdminModule.h"
#include "OSArrayObjectDeleter.h"
#include "StringParser.h"
#include "QTSSModuleUtils.h"
#include "OSMutex.h"
#include "AdminElementNode.h"
#include "base64.h"

#if __MacOSX__
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#endif

#if __solaris__ || __linux__ || __sgi__ || __hpux__
#include <crypt.h>
#endif

#define DEBUG_ADMIN_MODULE 0

//**************************************************
#define kAuthNameAndPasswordBuffSize 512
#define kPasswordBuffSize kAuthNameAndPasswordBuffSize/2

// STATIC DATA
//**************************************************
#if DEBUG_ADMIN_MODULE
static UInt32	sRequestCount = 0;
#endif

static QTSS_Initialize_Params sQTSSparams;

//static char* sResponseHeader = "HTTP/1.0 200 OK\r\nServer: QTSS\r\nConnection: Close\r\nContent-Type: text/plain\r\n\r\n";
static char *sResponseHeader = "HTTP/1.0 200 OK";
static char *sUnauthorizedResponseHeader =
    "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"QTSS/modules/admin\"\r\nServer: QTSS\r\nConnection: Close\r\nContent-Type: text/plain\r\n\r\n";
static char *sPermissionDeniedHeader =
    "HTTP/1.1 403 Forbidden\r\nConnection: Close\r\nContent-Type: text/html\r\n\r\n";
static char *sHTMLBody =
    "<HTML><BODY>\n<P><b>Your request was denied by the server.</b></P>\n</BODY></HTML>\r\n\r\n";

static char *sVersionHeader = NULL;
static char *sConnectionHeader = "Connection: Close";
static char *kDefaultHeader = "Server: EasyDarwin";
static char *sContentType = "Content-Type: text/plain";
static char *sEOL = "\r\n";
static char *sEOM = "\r\n\r\n";
static char *sAuthRealm = "QTSS/modules/admin";
static char *sAuthResourceLocalPath = "/modules/admin/";

static QTSS_ServerObject sServer = NULL;
static QTSS_ModuleObject sModule = NULL;
static QTSS_ModulePrefsObject sAdminPrefs = NULL;
static QTSS_ModulePrefsObject sAccessLogPrefs = NULL;
static QTSS_ModulePrefsObject sReflectorPrefs = NULL;
static QTSS_ModulePrefsObject sHLSModulePrefs = NULL;

static QTSS_PrefsObject sServerPrefs = NULL;
static AdminClass *sAdminPtr = NULL;
static QueryURI *sQueryPtr = NULL;
static OSMutex *sAdminMutex = NULL;//admin module isn't reentrant
static UInt32 sVersion = 20030306;
static char *sDesc =
    "Implements HTTP based Admin Protocol for accessing server attributes";
static char decodedLine[kAuthNameAndPasswordBuffSize] = {0};
static char codedLine[kAuthNameAndPasswordBuffSize] = {0};
static QTSS_TimeVal sLastRequestTime = 0;
static UInt32 sSessID = 0;

static StrPtrLen sAuthRef("AuthRef");
#if __MacOSX__

static char*                sSecurityServerAuthKey = "com.apple.server.admin.streaming";
static AuthorizationItem    sRight = { sSecurityServerAuthKey, 0, NULL, 0 };
static AuthorizationRights  sRightSet = { 1, &sRight };
#endif

//ATTRIBUTES
//**************************************************
enum {
  kMaxRequestTimeIntervalMilli = 1000,
  kDefaultRequestTimeIntervalMilli = 50
};
static UInt32
    sDefaultRequestTimeIntervalMilli = kDefaultRequestTimeIntervalMilli;
static UInt32 sRequestTimeIntervalMilli = kDefaultRequestTimeIntervalMilli;

static bool sAuthenticationEnabled = true;
static bool sDefaultAuthenticationEnabled = true;

static bool sLocalLoopBackOnlyEnabled = true;
static bool sDefaultLocalLoopBackOnlyEnabled = true;

static bool sEnableRemoteAdmin = true;
static bool sDefaultEnableRemoteAdmin = true;

static QTSS_AttributeID sIPAccessListID = qtssIllegalAttrID;
static char *sIPAccessList = NULL;
static char *sLocalLoopBackAddress = "127.0.0.*";

static char *sAdministratorGroup = NULL;
static char *sDefaultAdministratorGroup = "admin";

static bool sFlushing = false;
static QTSS_AttributeID sFlushingID = qtssIllegalAttrID;
static char *sFlushingName = "QTSSAdminModuleFlushingState";
static UInt32 sFlushingLen = sizeof(sFlushing);

static QTSS_AttributeID sAuthenticatedID = qtssIllegalAttrID;
static char *sAuthenticatedName = "QTSSAdminModuleAuthenticatedState";

static QTSS_Error QTSSAdminModuleDispatch(QTSS_Role inRole,
                                          QTSS_RoleParamPtr inParams);
static QTSS_Error Register(QTSS_Register_Params *inParams);
static QTSS_Error Initialize(QTSS_Initialize_Params *inParams);
static QTSS_Error FilterRequest(QTSS_Filter_Params *inParams);
static QTSS_Error RereadPrefs();
static QTSS_Error AuthorizeAdminRequest(QTSS_RTSPRequestObject request);
static bool AcceptSession(QTSS_RTSPSessionObject inRTSPSession);

#if !DEBUG_ADMIN_MODULE
#define APITests_DEBUG()
#define ShowQuery_DEBUG()
#else
void ShowQuery_DEBUG()
{
    s_printf("======REQUEST #%"   _U32BITARG_   "======\n", ++sRequestCount);
    StrPtrLen*  aStr;
    aStr = sQueryPtr->GetURL();
    s_printf("URL="); PRINT_STR(aStr);

    aStr = sQueryPtr->GetQuery();
    s_printf("Query="); PRINT_STR(aStr);

    aStr = sQueryPtr->GetParameters();
    s_printf("Parameters="); PRINT_STR(aStr);

    aStr = sQueryPtr->GetCommand();
    s_printf("Command="); PRINT_STR(aStr);
    s_printf("CommandID=%" _S32BITARG_ " \n", sQueryPtr->GetCommandID());
    aStr = sQueryPtr->GetValue();
    s_printf("Value="); PRINT_STR(aStr);
    aStr = sQueryPtr->GetType();
    s_printf("Type="); PRINT_STR(aStr);
    aStr = sQueryPtr->GetAccess();
    s_printf("Access="); PRINT_STR(aStr);
}

void APITests_DEBUG()
{
    if (0)
    {
        s_printf("QTSSAdminModule start tests \n");

        if (0)
        {
            s_printf("admin called locked \n");
            const int ksleeptime = 15;
            s_printf("sleeping for %d seconds \n", ksleeptime);
            sleep(ksleeptime);
            s_printf("done sleeping \n");
            s_printf("QTSS_GlobalUnLock \n");
            (void)QTSS_GlobalUnLock();
            s_printf("again sleeping for %d seconds \n", ksleeptime);
            sleep(ksleeptime);
        }

        if (0)
        {
            s_printf(" GET VALUE PTR TEST \n");

            QTSS_Object *sessionsPtr = NULL;
            UInt32      paramLen = sizeof(sessionsPtr);
            UInt32      numValues = 0;
            QTSS_Error  err = 0;

            err = QTSS_GetNumValues(sServer, qtssSvrClientSessions, &numValues);
            err = QTSS_GetValuePtr(sServer, qtssSvrClientSessions, 0, (void**)&sessionsPtr, &paramLen);
            s_printf("Admin Module Num Sessions = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ " err = %" _S32BITARG_ " paramLen =%"   _U32BITARG_   "\n", numValues, (SInt32)*sessionsPtr, err, paramLen);

            UInt32      numAttr = 0;
            if (sessionsPtr)
            {
                err = QTSS_GetNumAttributes(*sessionsPtr, &numAttr);
                s_printf("Admin Module Num attributes = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ "  err = %" _S32BITARG_ "\n", numAttr, (SInt32)*sessionsPtr, err);

                QTSS_Object theAttributeInfo;
                char nameBuff[128];
                UInt32 len = 127;
                for (UInt32 i = 0; i < numAttr; i++)
                {
                    err = QTSS_GetAttrInfoByIndex(*sessionsPtr, i, &theAttributeInfo);
                    nameBuff[0] = 0; len = 127;
                    err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
                    nameBuff[len] = 0;
                    s_printf("found %s \n", nameBuff);
                }
            }
        }

        if (0)
        {
            s_printf(" GET VALUE TEST \n");

            QTSS_Object sessions = NULL;
            UInt32      paramLen = sizeof(sessions);
            UInt32      numValues = 0;
            QTSS_Error  err = 0;

            err = QTSS_GetNumValues(sServer, qtssSvrClientSessions, &numValues);
            err = QTSS_GetValue(sServer, qtssSvrClientSessions, 0, (void*)&sessions, &paramLen);
            s_printf("Admin Module Num Sessions = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ " err = %" _S32BITARG_ " paramLen = %"   _U32BITARG_   "\n", numValues, (SInt32)sessions, err, paramLen);

            if (sessions)
            {
                UInt32      numAttr = 0;
                err = QTSS_GetNumAttributes(sessions, &numAttr);
                s_printf("Admin Module Num attributes = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ "  err = %" _S32BITARG_ "\n", numAttr, (SInt32)sessions, err);

                QTSS_Object theAttributeInfo;
                char nameBuff[128];
                UInt32 len = 127;
                for (UInt32 i = 0; i < numAttr; i++)
                {
                    err = QTSS_GetAttrInfoByIndex(sessions, i, &theAttributeInfo);
                    nameBuff[0] = 0; len = 127;
                    err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
                    nameBuff[len] = 0;
                    s_printf("found %s \n", nameBuff);
                }
            }
        }


        if (0)
        {
            s_printf("----------------- Start test ----------------- \n");
            s_printf(" GET indexed pref TEST \n");

            QTSS_Error  err = 0;

            UInt32      numAttr = 1;
            err = QTSS_GetNumAttributes(sAdminPrefs, &numAttr);
            s_printf("Admin Module Num preference attributes = %"   _U32BITARG_   " err = %" _S32BITARG_ "\n", numAttr, err);

            QTSS_Object theAttributeInfo;
            char valueBuff[512];
            char nameBuff[128];
            QTSS_AttributeID theID;
            UInt32 len = 127;
            UInt32 i = 0;
            s_printf("first pass over preferences\n");
            for (i = 0; i < numAttr; i++)
            {
                err = QTSS_GetAttrInfoByIndex(sAdminPrefs, i, &theAttributeInfo);
                nameBuff[0] = 0; len = 127;
                err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
                nameBuff[len] = 0;

                theID = qtssIllegalAttrID; len = sizeof(theID);
                err = QTSS_GetValue(theAttributeInfo, qtssAttrID, 0, &theID, &len);
                s_printf("found preference=%s \n", nameBuff);
            }
            valueBuff[0] = 0; len = 512;
            err = QTSS_GetValue(sAdminPrefs, theID, 0, valueBuff, &len); valueBuff[len] = 0;
            s_printf("Admin Module QTSS_GetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);
            err = QTSS_SetValue(sAdminPrefs, theID, 0, valueBuff, len);
            s_printf("Admin Module QTSS_SetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);

            {   QTSS_ServiceID id;
            (void)QTSS_IDForService(QTSS_REREAD_PREFS_SERVICE, &id);
            (void)QTSS_DoService(id, NULL);
            }

            valueBuff[0] = 0; len = 512;
            err = QTSS_GetValue(sAdminPrefs, theID, 0, valueBuff, &len); valueBuff[len] = 0;
            s_printf("Admin Module QTSS_GetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);
            err = QTSS_SetValue(sAdminPrefs, theID, 0, valueBuff, len);
            s_printf("Admin Module QTSS_SetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);

            s_printf("second pass over preferences\n");
            for (i = 0; i < numAttr; i++)
            {
                err = QTSS_GetAttrInfoByIndex(sAdminPrefs, i, &theAttributeInfo);
                nameBuff[0] = 0; len = 127;
                err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
                nameBuff[len] = 0;

                theID = qtssIllegalAttrID; len = sizeof(theID);
                err = QTSS_GetValue(theAttributeInfo, qtssAttrID, 0, &theID, &len);
                s_printf("found preference=%s \n", nameBuff);
            }
            s_printf("----------------- Done test ----------------- \n");
        }

    }
}

#endif

inline void KeepSession(QTSS_RTSPRequestObject theRequest, bool keep) {
  (void) QTSS_SetValue(theRequest, qtssRTSPReqRespKeepAlive, 0, &keep, sizeof(keep));
}

// FUNCTION IMPLEMENTATIONS

QTSS_Error QTSSAdminModule_Main(void *inPrivateArgs) {
  return _stublibrary_main(inPrivateArgs, QTSSAdminModuleDispatch);
}

QTSS_Error QTSSAdminModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams) {
  switch (inRole) {
    case QTSS_Register_Role: return Register(&inParams->regParams);
    case QTSS_Initialize_Role: return Initialize(&inParams->initParams);
    case QTSS_RTSPFilter_Role: {
      if (!sEnableRemoteAdmin)
        break;
      return FilterRequest(&inParams->rtspFilterParams);
    }
    case QTSS_RTSPAuthorize_Role: return AuthorizeAdminRequest(inParams->rtspRequestParams.inRTSPRequest);
    case QTSS_RereadPrefs_Role: return RereadPrefs();
  }
  return QTSS_NoErr;
}

QTSS_Error Register(QTSS_Register_Params *inParams) {
  // Do role & attribute setup
  (void) QTSS_AddRole(QTSS_Initialize_Role);
  (void) QTSS_AddRole(QTSS_RTSPFilter_Role);
  (void) QTSS_AddRole(QTSS_RereadPrefs_Role);
  (void) QTSS_AddRole(QTSS_RTSPAuthorize_Role);

  (void) QTSS_AddStaticAttribute(qtssRTSPRequestObjectType,
                                 sFlushingName,
                                 NULL,
                                 qtssAttrDataTypeBool16);
  (void) QTSS_IDForAttr(qtssRTSPRequestObjectType, sFlushingName, &sFlushingID);

  (void) QTSS_AddStaticAttribute(qtssRTSPRequestObjectType,
                                 sAuthenticatedName,
                                 NULL,
                                 qtssAttrDataTypeBool16);
  (void) QTSS_IDForAttr(qtssRTSPRequestObjectType,
                        sAuthenticatedName,
                        &sAuthenticatedID);

  // Tell the server our name!
  static char *sModuleName = "QTSSAdminModule";
  ::strcpy(inParams->outModuleName, sModuleName);

  return QTSS_NoErr;
}

QTSS_Error RereadPrefs() {

  delete[] sVersionHeader;
  sVersionHeader = QTSSModuleUtils::GetStringAttribute(sServer,
                                                       "qtssSvrRTSPServerHeader",
                                                       kDefaultHeader);

  delete[] sIPAccessList;
  sIPAccessList = QTSSModuleUtils::GetStringAttribute(sAdminPrefs,
                                                      "IPAccessList",
                                                      sLocalLoopBackAddress);
  sIPAccessListID = QTSSModuleUtils::GetAttrID(sAdminPrefs, "IPAccessList");

  QTSSModuleUtils::GetAttribute(sAdminPrefs,
                                "Authenticate",
                                qtssAttrDataTypeBool16,
                                &sAuthenticationEnabled,
                                &sDefaultAuthenticationEnabled,
                                sizeof(sAuthenticationEnabled));
  QTSSModuleUtils::GetAttribute(sAdminPrefs,
                                "LocalAccessOnly",
                                qtssAttrDataTypeBool16,
                                &sLocalLoopBackOnlyEnabled,
                                &sDefaultLocalLoopBackOnlyEnabled,
                                sizeof(sLocalLoopBackOnlyEnabled));
  QTSSModuleUtils::GetAttribute(sAdminPrefs,
                                "RequestTimeIntervalMilli",
                                qtssAttrDataTypeUInt32,
                                &sRequestTimeIntervalMilli,
                                &sDefaultRequestTimeIntervalMilli,
                                sizeof(sRequestTimeIntervalMilli));
  QTSSModuleUtils::GetAttribute(sAdminPrefs,
                                "enable_remote_admin",
                                qtssAttrDataTypeBool16,
                                &sEnableRemoteAdmin,
                                &sDefaultEnableRemoteAdmin,
                                sizeof(sDefaultEnableRemoteAdmin));

  delete[] sAdministratorGroup;
  sAdministratorGroup = QTSSModuleUtils::GetStringAttribute(sAdminPrefs,
                                                            "AdministratorGroup",
                                                            sDefaultAdministratorGroup);

  if (sRequestTimeIntervalMilli > kMaxRequestTimeIntervalMilli) {
    sRequestTimeIntervalMilli = kMaxRequestTimeIntervalMilli;
  }

  (void) QTSS_SetValue(sModule, qtssModDesc, 0, sDesc, strlen(sDesc) + 1);
  (void) QTSS_SetValue(sModule, qtssModVersion, 0, &sVersion, sizeof(sVersion));

  return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params *inParams) {
  sAdminMutex = new OSMutex();
  ElementNode_InitPtrArray();
  // Setup module utils
  QTSSModuleUtils::Initialize(inParams->inMessages,
                              inParams->inServer,
                              inParams->inErrorLogStream);

  sQTSSparams = *inParams;
  sServer = inParams->inServer;
  sModule = inParams->inModule;

  sAccessLogPrefs =
      QTSSModuleUtils::GetModulePrefsObject(QTSSModuleUtils::GetModuleObjectByName(
          "QTSSAccessLogModule"));
  sReflectorPrefs =
      QTSSModuleUtils::GetModulePrefsObject(QTSSModuleUtils::GetModuleObjectByName(
          "QTSSReflectorModule"));
  sHLSModulePrefs =
      QTSSModuleUtils::GetModulePrefsObject(QTSSModuleUtils::GetModuleObjectByName(
          "EasyHLSModule"));

  sAdminPrefs = QTSSModuleUtils::GetModulePrefsObject(sModule);
  sServerPrefs = inParams->inPrefs;

  RereadPrefs();

  return QTSS_NoErr;
}

void ReportErr(QTSS_Filter_Params *inParams, UInt32 err) {
  StrPtrLen *urlPtr = sQueryPtr->GetURL();
  StrPtrLen *evalMessagePtr = sQueryPtr->GetEvalMsg();
  char temp[32];

  if (urlPtr && evalMessagePtr) {
    s_sprintf(temp, "(%"   _U32BITARG_   ")", err);
    (void) QTSS_Write(inParams->inRTSPRequest,
                      "error:",
                      strlen("error:"),
                      NULL,
                      0);
    (void) QTSS_Write(inParams->inRTSPRequest, temp, strlen(temp), NULL, 0);
    if (sQueryPtr->VerboseParam()) {
      (void) QTSS_Write(inParams->inRTSPRequest,
                        ";URL=",
                        strlen(";URL="),
                        NULL,
                        0);
      if (urlPtr)
        (void) QTSS_Write(inParams->inRTSPRequest,
                          urlPtr->Ptr,
                          urlPtr->Len,
                          NULL,
                          0);
    }
    if (sQueryPtr->DebugParam()) {
      (void) QTSS_Write(inParams->inRTSPRequest, ";", strlen(";"), NULL, 0);
      (void) QTSS_Write(inParams->inRTSPRequest,
                        evalMessagePtr->Ptr,
                        evalMessagePtr->Len,
                        NULL,
                        0);
    }
    (void) QTSS_Write(inParams->inRTSPRequest, "\r\n\r\n", 4, NULL, 0);
  }
}

inline bool AcceptAddress(StrPtrLen *theAddressPtr) {
  IPComponentStr ipComponentStr(theAddressPtr);

  bool isLocalRequest = ipComponentStr.IsLocal();
  if (sLocalLoopBackOnlyEnabled && isLocalRequest)
    return true;

  if (sLocalLoopBackOnlyEnabled && !isLocalRequest)
    return false;

  if (QTSSModuleUtils::AddressInList(sAdminPrefs,
                                     sIPAccessListID,
                                     theAddressPtr))
    return true;

  return false;
}

inline bool IsAdminRequest(StringParser *theFullRequestPtr) {
  bool handleRequest = false;
  if (theFullRequestPtr != NULL)
    do {
      StrPtrLen strPtr;
      theFullRequestPtr->ConsumeWord(&strPtr);
      if (!strPtr.Equal(StrPtrLen("GET"))) break;   //it's a "Get" request

      theFullRequestPtr->ConsumeWhitespace();
      if (!theFullRequestPtr->Expect('/')) break;

      theFullRequestPtr->ConsumeWord(&strPtr);
      if (strPtr.Len == 0 || !strPtr.Equal(StrPtrLen("modules"))) break;
      if (!theFullRequestPtr->Expect('/')) break;

      theFullRequestPtr->ConsumeWord(&strPtr);
      if (strPtr.Len == 0 || !strPtr.Equal(StrPtrLen("admin"))) break;
      handleRequest = true;

    } while (false);

  return handleRequest;
}

inline void ParseAuthNameAndPassword(StrPtrLen *codedStrPtr,
                                     StrPtrLen *namePtr,
                                     StrPtrLen *passwordPtr) {
  if (!codedStrPtr || (codedStrPtr->Len >= kAuthNameAndPasswordBuffSize)) {
    return;
  }

  StrPtrLen codedLineStr;
  StrPtrLen nameAndPassword;
  memset(decodedLine, 0, kAuthNameAndPasswordBuffSize);
  memset(codedLine, 0, kAuthNameAndPasswordBuffSize);

  memcpy(codedLine, codedStrPtr->Ptr, codedStrPtr->Len);
  codedLineStr.Set((char *) codedLine, codedStrPtr->Len);
  (void) Base64decode(decodedLine, codedLineStr.Ptr);

  nameAndPassword.Set((char *) decodedLine, strlen(decodedLine));
  StringParser parsedNameAndPassword(&nameAndPassword);

  parsedNameAndPassword.ConsumeUntil(namePtr, ':');
  parsedNameAndPassword.ConsumeLength(NULL, 1);

  // password can have whitespace, so read until the end of the line, not just until whitespace
  parsedNameAndPassword.ConsumeUntil(passwordPtr, StringParser::sEOLMask);

  namePtr->Ptr[namePtr->Len] = 0;
  passwordPtr->Ptr[passwordPtr->Len] = 0;

  //s_printf("decoded nameAndPassword="); PRINT_STR(&nameAndPassword);
  //s_printf("decoded name="); PRINT_STR(namePtr);
  //s_printf("decoded password="); PRINT_STR(passwordPtr);

  return;
};

inline bool OSXAuthenticate(StrPtrLen *keyStrPtr) {
#if __MacOSX__
  //  Authorization: AuthRef QWxhZGRpbjpvcGVuIHNlc2FtZQ==
  bool result = false;

  if (keyStrPtr == NULL || keyStrPtr->Len == 0)
      return result;

  char *encodedKey = keyStrPtr->GetAsCString();
  OSCharArrayDeleter encodedKeyDeleter(encodedKey);

  char *decodedKey = new char[Base64decode_len(encodedKey) + 1];
  OSCharArrayDeleter decodedKeyDeleter(decodedKey);

  (void)Base64decode(decodedKey, encodedKey);

  AuthorizationExternalForm  *receivedExtFormPtr = (AuthorizationExternalForm  *)decodedKey;
  AuthorizationRef  receivedAuthorization;
  OSStatus status = AuthorizationCreateFromExternalForm(receivedExtFormPtr, &receivedAuthorization);

  if (status != errAuthorizationSuccess)
      return result;

  status = AuthorizationCopyRights(receivedAuthorization, &sRightSet, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights, NULL);
  if (status == errAuthorizationSuccess)
  {
      result = true;
  }

  AuthorizationFree(receivedAuthorization, kAuthorizationFlagDestroyRights);

  return result;

#else

  return false;

#endif

}

inline bool HasAuthentication(StringParser *theFullRequestPtr,
                              StrPtrLen *namePtr,
                              StrPtrLen *passwordPtr,
                              StrPtrLen *outAuthTypePtr) {
  //  Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
  bool hasAuthentication = false;
  StrPtrLen strPtr;
  StrPtrLen authType;
  StrPtrLen authString;
  while (theFullRequestPtr->GetDataRemaining() > 0) {
    theFullRequestPtr->ConsumeWhitespace();
    theFullRequestPtr->ConsumeUntilWhitespace(&strPtr);
    if (strPtr.Len == 0 || !strPtr.Equal(StrPtrLen("Authorization:")))
      continue;

    theFullRequestPtr->ConsumeWhitespace();
    theFullRequestPtr->ConsumeUntilWhitespace(&authType);
    if (authType.Len == 0)
      continue;

    theFullRequestPtr->ConsumeWhitespace();
    theFullRequestPtr->ConsumeUntil(&authString, StringParser::sEOLMask);
    if (authString.Len == 0)
      continue;

    if (outAuthTypePtr != NULL)
      outAuthTypePtr->Set(authType.Ptr, authType.Len);

    if (authType.Equal(StrPtrLen("Basic"))) {
      (void) ParseAuthNameAndPassword(&authString, namePtr, passwordPtr);
      if (namePtr->Len == 0)
        continue;

      hasAuthentication = true;
      break;
    } else if (authType.Equal(sAuthRef)) {
      namePtr->Set(NULL, 0);
      passwordPtr->Set(authString.Ptr, authString.Len);
      hasAuthentication = true;
      break;
    }
  };

  return hasAuthentication;
}

bool Authenticate(QTSS_RTSPRequestObject request,
                  StrPtrLen *namePtr,
                  StrPtrLen *passwordPtr) {
  bool authenticated = true;

  char *authName = namePtr->GetAsCString();
  OSCharArrayDeleter authNameDeleter(authName);

  QTSS_ActionFlags authAction = qtssActionFlagsAdmin;

  // authenticate callback to retrieve the password
  QTSS_Error err = QTSS_Authenticate(authName,
                                     sAuthResourceLocalPath,
                                     sAuthResourceLocalPath,
                                     authAction,
                                     qtssAuthBasic,
                                     request);
  if (err != QTSS_NoErr) {
    return false; // Couldn't even call QTSS_Authenticate...abandon!
  }

  // Get the user profile object from the request object that was created in the authenticate callback
  QTSS_UserProfileObject theUserProfile = NULL;
  UInt32 len = sizeof(QTSS_UserProfileObject);
  err = QTSS_GetValue(request,
                      qtssRTSPReqUserProfile,
                      0,
                      (void *) &theUserProfile,
                      &len);
  Assert(len == sizeof(QTSS_UserProfileObject));
  if (err != QTSS_NoErr)
    authenticated = false;

  if (err == QTSS_NoErr) {
    char *reqPassword = passwordPtr->GetAsCString();
    OSCharArrayDeleter reqPasswordDeleter(reqPassword);
    char *userPassword = NULL;
    (void) QTSS_GetValueAsString(theUserProfile,
                                 qtssUserPassword,
                                 0,
                                 &userPassword);
    OSCharArrayDeleter userPasswordDeleter(userPassword);

    if (userPassword == NULL) {
      authenticated = false;
    } else {
#ifdef __Win32__
      // The password is md5 encoded for win32
      char md5EncodeResult[120];
      MD5Encode(reqPassword, userPassword, md5EncodeResult, sizeof(md5EncodeResult));
      if (::strcmp(userPassword, md5EncodeResult) != 0)
          authenticated = false;
#else
      if (::strcmp(userPassword, (char *) crypt(reqPassword, userPassword))
          != 0)
        authenticated = false;
#endif
    }
  }

  char *realm = NULL;
  bool allowed = true;
  //authorize callback to check authorization
  // allocates memory for realm
  err = QTSS_Authorize(request, &realm, &allowed);
  // QTSS_Authorize allocates memory for the realm string
  // we don't use the realm returned by the callback, but instead
  // use our own.
  // delete the memory allocated for realm because we don't need it!
  OSCharArrayDeleter realmDeleter(realm);

  if (err != QTSS_NoErr) {
    s_printf("QTSSAdminModule::Authenticate: QTSS_Authorize failed\n");
    return false; // Couldn't even call QTSS_Authorize...abandon!
  }

  if (authenticated && allowed)
    return true;

  return false;
}

QTSS_Error AuthorizeAdminRequest(QTSS_RTSPRequestObject request) {
  bool allowed = false;

  // get the resource path
  // if the path does not match the admin path, don't handle the request
  char *resourcePath = QTSSModuleUtils::GetLocalPath_Copy(request);
  OSCharArrayDeleter resourcePathDeleter(resourcePath);

  if (strcmp(sAuthResourceLocalPath, resourcePath) != 0)
    return QTSS_NoErr;

  // get the type of request
  QTSS_ActionFlags action = QTSSModuleUtils::GetRequestActions(request);
  if (!(action & qtssActionFlagsAdmin))
    return QTSS_RequestFailed;

  QTSS_UserProfileObject
      theUserProfile = QTSSModuleUtils::GetUserProfileObject(request);
  if (NULL == theUserProfile)
    return QTSS_RequestFailed;

  (void) QTSS_SetValue(request,
                       qtssRTSPReqURLRealm,
                       0,
                       sAuthRealm,
                       ::strlen(sAuthRealm));

  // Authorize the user if the user belongs to the AdministratorGroup (this is an admin module pref)
  UInt32 numGroups = 0;
  char **groupsArray =
      QTSSModuleUtils::GetGroupsArray_Copy(theUserProfile, &numGroups);

  if ((groupsArray != NULL) && (numGroups != 0)) {
    UInt32 index = 0;
    for (index = 0; index < numGroups; index++) {
      if (strcmp(sAdministratorGroup, groupsArray[index]) == 0) {
        allowed = true;
        break;
      }
    }

    // delete the memory allocated in QTSSModuleUtils::GetGroupsArray_Copy call
    delete[] groupsArray;
  }

  if (!allowed) {
    if (QTSS_NoErr != QTSS_SetValue(request, qtssRTSPReqUserAllowed, 0, &allowed, sizeof(allowed)))
      return QTSS_RequestFailed; // Bail on the request. The Server will handle the error
  }

  return QTSS_NoErr;
}

bool AcceptSession(QTSS_RTSPSessionObject inRTSPSession) {
  char remoteAddress[20] = {0};
  StrPtrLen theClientIPAddressStr(remoteAddress, sizeof(remoteAddress));
  QTSS_Error err = QTSS_GetValue(
      inRTSPSession, qtssRTSPSesRemoteAddrStr, 0, (void *) theClientIPAddressStr.Ptr, &theClientIPAddressStr.Len);
  if (err != QTSS_NoErr) return false;

  return AcceptAddress(&theClientIPAddressStr);
}

bool StillFlushing(QTSS_Filter_Params *inParams, bool flushing) {

  QTSS_Error err = QTSS_NoErr;
  if (flushing) {
    err = QTSS_Flush(inParams->inRTSPRequest);
    //s_printf("Flushing session=%"   _U32BITARG_   " QTSS_Flush err =%" _S32BITARG_ "\n",sSessID,err);
  }
  if (err == QTSS_WouldBlock) { // more to flush later
    sFlushing = true;
    (void) QTSS_SetValue(inParams->inRTSPRequest, sFlushingID, 0, (void *) &sFlushing, sFlushingLen);
    err = QTSS_RequestEvent(inParams->inRTSPRequest, QTSS_WriteableEvent);
    KeepSession(inParams->inRTSPRequest, true);
    //s_printf("Flushing session=%"   _U32BITARG_   " QTSS_RequestEvent err =%" _S32BITARG_ "\n",sSessID,err);
  } else {
    sFlushing = false;
    (void) QTSS_SetValue(inParams->inRTSPRequest, sFlushingID, 0, (void *) &sFlushing, sFlushingLen);
    KeepSession(inParams->inRTSPRequest, false);

    if (flushing) { // we were flushing so reset the LastRequestTime
      sLastRequestTime = QTSS_Milliseconds();
      //s_printf("Done Flushing session=%"   _U32BITARG_   "\n",sSessID);
      return true;
    }
  }

  return sFlushing;
}

bool IsAuthentic(QTSS_Filter_Params *inParams, StringParser *fullRequestPtr) {
  bool isAuthentic = false;

  if (!sAuthenticationEnabled) { // no authentication
    isAuthentic = true;
  } else { // must authenticate
    StrPtrLen theClientIPAddressStr;
    (void) QTSS_GetValuePtr(inParams->inRTSPSession, qtssRTSPSesRemoteAddrStr, 0,
                            (void **) &theClientIPAddressStr.Ptr, &theClientIPAddressStr.Len);
    bool isLocal = IPComponentStr(&theClientIPAddressStr).IsLocal();

    StrPtrLen authenticateName;
    StrPtrLen authenticatePassword;
    StrPtrLen authType;
    bool hasAuthentication = HasAuthentication(fullRequestPtr, &authenticateName, &authenticatePassword, &authType);
    if (hasAuthentication) {
      if (authType.Equal(sAuthRef)) {
        if (isLocal)
          isAuthentic = OSXAuthenticate(&authenticatePassword);
      } else
        isAuthentic = Authenticate(inParams->inRTSPRequest, &authenticateName, &authenticatePassword);
    }
  }
  //    if (isAuthentic)
  //        isAuthentic = AuthorizeAdminRequest(inParams->inRTSPRequest);
  (void) QTSS_SetValue(inParams->inRTSPRequest, sAuthenticatedID, 0, (void *) &isAuthentic, sizeof(isAuthentic));

  return isAuthentic;
}

inline bool InWaitInterval(QTSS_Filter_Params *inParams) {
  QTSS_TimeVal nextExecuteTime = sLastRequestTime + sRequestTimeIntervalMilli;
  QTSS_TimeVal currentTime = QTSS_Milliseconds();
  if (currentTime < nextExecuteTime) {
    SInt32 waitTime = (SInt32) (nextExecuteTime - currentTime) + 1;
    //s_printf("(currentTime < nextExecuteTime) sSessID = %"   _U32BITARG_   " waitTime =%" _S32BITARG_ " currentTime = %qd nextExecute = %qd interval=%"   _U32BITARG_   "\n",sSessID, waitTime, currentTime, nextExecuteTime,sRequestTimeIntervalMilli);
    (void) QTSS_SetIdleTimer(waitTime);
    KeepSession(inParams->inRTSPRequest, true);

    //s_printf("-- call me again after %" _S32BITARG_ " millisecs session=%"   _U32BITARG_   " \n",waitTime,sSessID);
    return true;
  }
  sLastRequestTime = QTSS_Milliseconds();
  //s_printf("handle sessID=%"   _U32BITARG_   " time=%qd \n",sSessID,currentTime);
  return false;
}

inline void GetQueryData(QTSS_RTSPRequestObject theRequest) {
  sAdminPtr = new AdminClass();
  Assert(sAdminPtr != NULL);
  if (sAdminPtr == NULL) {   //s_printf ("new AdminClass() failed!! \n");
    return;
  }
  if (sAdminPtr != NULL) {
    sAdminPtr->Initialize(&sQTSSparams, sQueryPtr);  // Get theData
  }
}

inline void SendHeader(QTSS_StreamRef inStream) {
  (void) QTSS_Write(inStream,
                    sResponseHeader,
                    ::strlen(sResponseHeader),
                    NULL,
                    0);
  (void) QTSS_Write(inStream, sEOL, ::strlen(sEOL), NULL, 0);
  (void) QTSS_Write(inStream,
                    sVersionHeader,
                    ::strlen(sVersionHeader),
                    NULL,
                    0);
  (void) QTSS_Write(inStream, sEOL, ::strlen(sEOL), NULL, 0);
  (void) QTSS_Write(inStream,
                    sConnectionHeader,
                    ::strlen(sConnectionHeader),
                    NULL,
                    0);
  (void) QTSS_Write(inStream, sEOL, ::strlen(sEOL), NULL, 0);
  (void) QTSS_Write(inStream, sContentType, ::strlen(sContentType), NULL, 0);
  (void) QTSS_Write(inStream, sEOM, ::strlen(sEOM), NULL, 0);
}

inline void SendResult(QTSS_StreamRef inStream) {
  SendHeader(inStream);
  if (sAdminPtr != NULL)
    sAdminPtr->RespondToQuery(inStream, sQueryPtr, sQueryPtr->GetRootID());

}

inline bool GetRequestAuthenticatedState(QTSS_Filter_Params *inParams) {
  bool result = false;
  UInt32 paramLen = sizeof(result);
  QTSS_Error err = QTSS_GetValue(inParams->inRTSPRequest,
                                 sAuthenticatedID,
                                 0,
                                 (void *) &result,
                                 &paramLen);
  if (err != QTSS_NoErr) {
    paramLen = sizeof(result);
    result = false;
    err = QTSS_SetValue(inParams->inRTSPRequest,
                        sAuthenticatedID,
                        0,
                        (void *) &result,
                        paramLen);
  }
  return result;
}

inline bool GetRequestFlushState(QTSS_Filter_Params *inParams) {
  bool result = false;
  UInt32 paramLen = sizeof(result);
  QTSS_Error err = QTSS_GetValue(inParams->inRTSPRequest,
                                 sFlushingID,
                                 0,
                                 (void *) &result,
                                 &paramLen);
  if (err != QTSS_NoErr) {
    paramLen = sizeof(result);
    result = false;
    //s_printf("no flush val so set to false session=%"   _U32BITARG_   " err =%" _S32BITARG_ "\n",sSessID, err);
    err = QTSS_SetValue(inParams->inRTSPRequest,
                        sFlushingID,
                        0,
                        (void *) &result,
                        paramLen);
    //s_printf("QTSS_SetValue flush session=%"   _U32BITARG_   " err =%" _S32BITARG_ "\n",sSessID, err);
  }
  return result;
}

QTSS_Error FilterRequest(QTSS_Filter_Params *inParams) {
  if (NULL == inParams || NULL == inParams->inRTSPSession
      || NULL == inParams->inRTSPRequest) {
    Assert(0);
    return QTSS_NoErr;
  }

  OSMutexLocker locker(sAdminMutex);
  //check to see if we should handle this request. Invokation is triggered
  //by a "GET /" request

  QTSS_RTSPRequestObject theRequest = inParams->inRTSPRequest;

  UInt32 paramLen = sizeof(sSessID);
  QTSS_Error err = QTSS_GetValue(inParams->inRTSPSession,
                                 qtssRTSPSesID,
                                 0,
                                 (void *) &sSessID,
                                 &paramLen);
  if (err != QTSS_NoErr)
    return QTSS_NoErr;

  StrPtrLen theFullRequest;
  err = QTSS_GetValuePtr(theRequest,
                         qtssRTSPReqFullRequest,
                         0,
                         (void **) &theFullRequest.Ptr,
                         &theFullRequest.Len);
  if (err != QTSS_NoErr)
    return QTSS_NoErr;

  StringParser fullRequest(&theFullRequest);

  if (!IsAdminRequest(&fullRequest))
    return QTSS_NoErr;

  if (!AcceptSession(inParams->inRTSPSession)) {
    (void) QTSS_Write(inParams->inRTSPRequest,
                      sPermissionDeniedHeader,
                      ::strlen(sPermissionDeniedHeader),
                      NULL,
                      0);
    (void) QTSS_Write(inParams->inRTSPRequest,
                      sHTMLBody,
                      ::strlen(sHTMLBody),
                      NULL,
                      0);
    KeepSession(theRequest, false);
    return QTSS_NoErr;
  }

  if (!GetRequestAuthenticatedState(inParams)) // must authenticate before handling
  {
    if (QTSS_IsGlobalLocked()) // must NOT be global locked
      return QTSS_RequestFailed;

    if (!IsAuthentic(inParams, &fullRequest)) {
      (void) QTSS_Write(inParams->inRTSPRequest,
                        sUnauthorizedResponseHeader,
                        ::strlen(sUnauthorizedResponseHeader),
                        NULL,
                        0);
      (void) QTSS_Write(inParams->inRTSPRequest,
                        sHTMLBody,
                        ::strlen(sHTMLBody),
                        NULL,
                        0);
      KeepSession(theRequest, false);
      return QTSS_NoErr;
    }

  }

  if (GetRequestFlushState(inParams)) {
    StillFlushing(inParams, true);
    return QTSS_NoErr;
  }

  if (!QTSS_IsGlobalLocked()) {
    if (InWaitInterval(inParams))
      return QTSS_NoErr;

    //s_printf("New Request Wait for GlobalLock session=%"   _U32BITARG_   "\n",sSessID);
    (void) QTSS_RequestGlobalLock();
    KeepSession(theRequest, true);
    return QTSS_NoErr;
  }

  //s_printf("Handle request session=%"   _U32BITARG_   "\n",sSessID);
  APITests_DEBUG();

  if (sQueryPtr != NULL) {
    delete sQueryPtr;
    sQueryPtr = NULL;
  }
  sQueryPtr = new QueryURI(&theFullRequest);
  if (sQueryPtr == NULL) return QTSS_NoErr;

  ShowQuery_DEBUG();

  if (sAdminPtr != NULL) {
    delete sAdminPtr;
    sAdminPtr = NULL;
  }
  UInt32 result = sQueryPtr->EvalQuery(NULL, NULL);
  if (result == 0)
    do {
      if (ElementNode_CountPtrs() > 0) {
        ElementNode_ShowPtrs();
        Assert(0);
      }

      GetQueryData(theRequest);

      SendResult(theRequest);
      delete sAdminPtr;
      sAdminPtr = NULL;

      if (sQueryPtr && !sQueryPtr->QueryHasReponse()) {
        UInt32 err = 404;
        (void) sQueryPtr->EvalQuery(&err, NULL);
        ReportErr(inParams, err);
        break;
      }

      if (sQueryPtr && sQueryPtr->QueryHasReponse()) {
        ReportErr(inParams, sQueryPtr->GetEvaluResult());
      }

      if (sQueryPtr->fIsPref && sQueryPtr->GetEvaluResult() == 0) {
        QTSS_ServiceID id;
        (void) QTSS_IDForService(QTSS_REREAD_PREFS_SERVICE, &id);
        (void) QTSS_DoService(id, NULL);
      }
    } while (false);
  else {
    SendHeader(theRequest);
    ReportErr(inParams, sQueryPtr->GetEvaluResult());
  }

  if (sQueryPtr != NULL) {
    delete sQueryPtr;
    sQueryPtr = NULL;
  }

  (void) StillFlushing(inParams, true);
  return QTSS_NoErr;

}