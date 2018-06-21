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

#ifndef QTSS_H
#define QTSS_H

#include <string>

#include <CF/Types.h>
#include <CF/Net/Http/HTTPProtocol.h>

#include "QTSSRTSPProtocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !__Win32__ && !__MinGW__
#include <sys/uio.h>
#endif

#define QTSS_API_VERSION                 0x00050000
#define QTSS_MAX_MODULE_NAME_LENGTH     64
#define QTSS_MAX_SESSION_ID_LENGTH      64
#define QTSS_MAX_ATTRIBUTE_NAME_SIZE    64
#define QTSS_MAX_URL_LENGTH                 512
#define QTSS_MAX_NAME_LENGTH             128
#define QTSS_MAX_REQUEST_BUFFER_SIZE    (2*1024)
#define QTSS_MAX_ATTRIBUTE_NUMS         128

#define EASY_ACCENCODER_BUFFER_SIZE_LEN    (16*1024*4)
#define EASY_KEY_SPLITER                "-"

//*******************************
// ENUMERATED TYPES

/**
 * Error Codes
 */
enum {
  QTSS_NoErr = 0,
  QTSS_RequestFailed = -1,
  QTSS_Unimplemented = -2,
  QTSS_RequestArrived = -3,
  QTSS_OutOfState = -4,
  QTSS_NotAModule = -5,
  QTSS_WrongVersion = -6,
  QTSS_IllegalService = -7,
  QTSS_BadIndex = -8,
  QTSS_ValueNotFound = -9,
  QTSS_BadArgument = -10,
  QTSS_ReadOnly = -11,
  QTSS_NotPreemptiveSafe = -12,
  QTSS_NotEnoughSpace = -13,
  QTSS_WouldBlock = -14,
  QTSS_NotConnected = -15,
  QTSS_FileNotFound = -16,
  QTSS_NoMoreData = -17,
  QTSS_AttrDoesntExist = -18,
  QTSS_AttrNameExists = -19,
  QTSS_InstanceAttrsNotAllowed = -20,
  QTSS_UnknowAudioCoder = -21
};
typedef SInt32 QTSS_Error;

/**
 * QTSS_AddStreamFlags used in the QTSS_AddStream Callback function
 */
enum {
  qtssASFlagsNoFlags = 0x00000000,
  qtssASFlagsAllowDestination = 0x00000001,
  qtssASFlagsForceInterleave = 0x00000002,
  qtssASFlagsDontUseSlowStart = 0x00000004,
  qtssASFlagsForceUDPTransport = 0x00000008
};
typedef UInt32 QTSS_AddStreamFlags;

/**
 * QTSS_PlayFlags used in the QTSS_Play Callback function.
 */
enum {
  qtssPlayFlagsSendRTCP = 0x00000010,   // have the server generate RTCP Sender Reports
  qtssPlayFlagsAppendServerInfo = 0x00000020    // have the server append the server info APP packet to your RTCP Sender Reports
};
typedef UInt32 QTSS_PlayFlags;

/**
 * Flags for QTSS_Write when writing to a QTSS_ClientSessionObject.
 */
enum {
  qtssWriteFlagsNoFlags = 0x00000000,
  qtssWriteFlagsIsRTP = 0x00000001,
  qtssWriteFlagsIsRTCP = 0x00000002,
  qtssWriteFlagsWriteBurstBegin = 0x00000004,
  qtssWriteFlagsBufferData = 0x00000008
};
typedef UInt32 QTSS_WriteFlags;

/**
 * Flags for QTSS_SendStandardRTSPResponse
 */
enum {
  qtssPlayRespWriteTrackInfo = 0x00000001,
  qtssSetupRespDontWriteSSRC = 0x00000002
};

/**
 * Flags for the qtssRTSPReqAction attribute in a QTSS_RTSPRequestObject.
 */
enum {
  qtssActionFlagsNoFlags = 0x00000000,
  qtssActionFlagsRead = 0x00000001,
  qtssActionFlagsWrite = 0x00000002,
  qtssActionFlagsAdmin = 0x00000004,
  qtssActionFlagsExtended = 0x40000000,
  qtssActionQTSSExtended = 0x80000000,
};
typedef UInt32 QTSS_ActionFlags;

enum {
  easyRedisActionDelete = 0,
  easyRedisActionSet = 1
};
typedef UInt32 Easy_RedisAction;

/**
 * RTP SESSION STATES
 *
 * Is this session playing, paused, or what?
 */
enum {
  qtssPausedState = 0,
  qtssPlayingState = 1
};
typedef UInt32 QTSS_RTPSessionState;

/**
 * CLIENT SESSION CLOSING REASON
 *
 * Why is this Client going away?
 */
enum {
  qtssCliSesCloseClientTeardown = 0, // QTSS_Teardown was called on this session
  qtssCliSesCloseTimeout = 1, // Server is timing this session out
  qtssCliSesCloseClientDisconnect = 2  // Client disconnected.
};
typedef UInt32 QTSS_CliSesClosingReason;

/**
 * CLIENT SESSION TEARDOWN REASON
 *
 * An attribute in the QTSS_ClientSessionObject
 *
 * When calling QTSS_Teardown, a module should specify the QTSS_CliSesTeardownReason in the QTSS_ClientSessionObject
 *  if the tear down was not a client request.
 */
enum {
  qtssCliSesTearDownClientRequest = 0,
  qtssCliSesTearDownUnsupportedMedia = 1,
  qtssCliSesTearDownServerShutdown = 2,
  qtssCliSesTearDownServerInternalErr = 3,
  qtssCliSesTearDownBroadcastEnded = 4 // A broadcast the client was watching ended
};
typedef UInt32 QTSS_CliSesTeardownReason;

/**
 * Events
 */
enum {
  QTSS_ReadableEvent = 1,
  QTSS_WriteableEvent = 2
};
typedef UInt32 QTSS_EventType;

/**
 * Authentication schemes
 */
enum {
  qtssAuthNone = 0,
  qtssAuthBasic = 1,
  qtssAuthDigest = 2
};
typedef UInt32 QTSS_AuthScheme;


/**********************************/
// RTSP SESSION TYPES
//
// Is this a normal RTSP session or an RTSP / HTTP session?
enum {
  qtssRTSPSession = 0,
  qtssRTSPHTTPSession = 1,
  qtssRTSPHTTPInputSession = 2 //The input half of an RTSPHTTP session. These session types are usually very short lived.
};
typedef UInt32 QTSS_RTSPSessionType;

/**********************************/
//
// What type of RTP transport is being used for the RTP stream?
enum {
  qtssRTPTransportTypeUDP = 0,
  qtssRTPTransportTypeReliableUDP = 1,
  qtssRTPTransportTypeTCP = 2,
  qtssRTPTransportTypeUnknown = 3
};
typedef UInt32 QTSS_RTPTransportType;

/**********************************/
//
// What type of RTP network mode is being used for the RTP stream?
// unicast | multicast (mutually exclusive)
enum {
  qtssRTPNetworkModeDefault = 0, // not declared
  qtssRTPNetworkModeMulticast = 1,
  qtssRTPNetworkModeUnicast = 2
};
typedef UInt32 QTSS_RTPNetworkMode;

/**********************************/
//
// The transport mode in a SETUP request
enum {
  qtssRTPTransportModePlay = 0,
  qtssRTPTransportModeRecord = 1
};
typedef UInt32 QTSS_RTPTransportMode;

/**********************************/
// PAYLOAD TYPES
//
// When a module adds an RTP stream to a client session, it must specify
// the stream's payload type. This is so that other modules can find out
// this information in a generalized fashion. Here are the currently
// defined payload types
enum {
  qtssUnknownPayloadType = 0,
  qtssVideoPayloadType = 1,
  qtssAudioPayloadType = 2
};
typedef UInt32 QTSS_RTPPayloadType;

/**********************************/
// QTSS API OBJECT TYPES
enum {
  qtssDynamicObjectType = FOUR_CHARS_TO_INT('d', 'y', 'm', 'c'),        //dymc
  qtssRTPStreamObjectType = FOUR_CHARS_TO_INT('r', 's', 't', 'o'),      //rsto
  qtssClientSessionObjectType = FOUR_CHARS_TO_INT('c', 's', 'e', 'o'),  //cseo
  qtssRTSPSessionObjectType = FOUR_CHARS_TO_INT('s', 's', 'e', 'o'),    //sseo
  qtssRTSPRequestObjectType = FOUR_CHARS_TO_INT('s', 'r', 'q', 'o'),    //srqo
  qtssRTSPHeaderObjectType = FOUR_CHARS_TO_INT('s', 'h', 'd', 'o'),     //shdo
  qtssServerObjectType = FOUR_CHARS_TO_INT('s', 'e', 'r', 'o'),         //sero
  qtssPrefsObjectType = FOUR_CHARS_TO_INT('p', 'r', 'f', 'o'),          //prfo
  qtssTextMessagesObjectType = FOUR_CHARS_TO_INT('t', 'x', 't', 'o'),   //txto
  qtssFileObjectType = FOUR_CHARS_TO_INT('f', 'i', 'l', 'e'),           //file
  qtssModuleObjectType = FOUR_CHARS_TO_INT('m', 'o', 'd', 'o'),         //modo
  qtssModulePrefsObjectType = FOUR_CHARS_TO_INT('m', 'o', 'd', 'p'),    //modp
  qtssAttrInfoObjectType = FOUR_CHARS_TO_INT('a', 't', 't', 'r'),       //attr
  qtssUserProfileObjectType = FOUR_CHARS_TO_INT('u', 's', 'p', 'o'),    //uspo
  qtssConnectedUserObjectType = FOUR_CHARS_TO_INT('c', 'u', 's', 'r'),  //cusr
  easyHTTPSessionObjectType = FOUR_CHARS_TO_INT('e', 'h', 's', 'o')     //ehso
};
typedef UInt32 QTSS_ObjectType;

/**********************************/
// ERROR LOG VERBOSITIES
//
// This provides some information to the module on the priority or
// type of this error message.
//
// When modules write to the error log stream (see below),
// the verbosity is qtssMessageVerbosity.
enum {
  qtssFatalVerbosity = 0,
  qtssWarningVerbosity = 1,
  qtssMessageVerbosity = 2,
  qtssAssertVerbosity = 3,
  qtssDebugVerbosity = 4,

  qtssIllegalVerbosity = 5
};
typedef UInt32 QTSS_ErrorVerbosity;

enum {
  qtssOpenFileNoFlags = 0,
  qtssOpenFileAsync = 1,  // File stream will be asynchronous (read may return QTSS_WouldBlock)
  qtssOpenFileReadAhead = 2   // File stream will be used for a linear read through the file.
};
typedef UInt32 QTSS_OpenFileFlags;


/**********************************/
// SERVER STATES
//
//  An attribute in the QTSS_ServerObject returns the server state
//  as a QTSS_ServerState. Modules may also set the server state.
//
//  Setting the server state to qtssFatalErrorState, or qtssShuttingDownState
//  will cause the server to quit.
//
//  Setting the state to qtssRefusingConnectionsState will cause the server
//  to start refusing new connections.
enum {
  qtssStartingUpState = 0,
  qtssRunningState = 1,
  qtssRefusingConnectionsState = 2,
  qtssFatalErrorState = 3,//a fatal error has occurred, not shutting down yet
  qtssShuttingDownState = 4,
  qtssIdleState = 5 // Like refusing connections state, but will also kill any currently connected clients
};
typedef UInt32 QTSS_ServerState;

/**********************************/
// ILLEGAL ATTRIBUTE ID
enum {
  qtssIllegalAttrID = -1,
  qtssIllegalServiceID = -1
};

//*********************************/
// QTSS DON'T CALL SENDPACKETS AGAIN
// If this time is specified as the next packet time when returning
// from QTSS_SendPackets_Role, the module won't get called again in
// that role until another QTSS_Play is issued
enum {
  qtssDontCallSendPacketsAgain = -1
};

// DATA TYPES
enum {
  qtssAttrDataTypeUnknown = 0,
  qtssAttrDataTypeCharArray = 1,
  qtssAttrDataTypeBool16 = 2,
  qtssAttrDataTypeSInt16 = 3,
  qtssAttrDataTypeUInt16 = 4,
  qtssAttrDataTypeSInt32 = 5,
  qtssAttrDataTypeUInt32 = 6,
  qtssAttrDataTypeSInt64 = 7,
  qtssAttrDataTypeUInt64 = 8,
  qtssAttrDataTypeQTSS_Object = 9,
  qtssAttrDataTypeQTSS_StreamRef = 10,
  qtssAttrDataTypeFloat32 = 11,
  qtssAttrDataTypeFloat64 = 12,
  qtssAttrDataTypeVoidPointer = 13,
  qtssAttrDataTypeTimeVal = 14,

  qtssAttrDataTypeNumTypes = 15
};
typedef UInt32 QTSS_AttrDataType;

enum {
  qtssAttrModeRead = 1,
  qtssAttrModeWrite = 2,
  qtssAttrModePreempSafe = 4,
  qtssAttrModeInstanceAttrAllowed = 8,
  qtssAttrModeCacheable = 16,
  qtssAttrModeDelete = 32
};
typedef UInt32 QTSS_AttrPermission;

enum {
  qtssAttrRightNone = 0,
  qtssAttrRightRead = 1 << 0,
  qtssAttrRightWrite = 1 << 1,
  qtssAttrRightAdmin = 1 << 2,
  qtssAttrRightExtended = 1 << 30, // Set this flag in the qtssUserRights when defining a new right. The right is a string i.e. "myauthmodule.myright" store the string in the QTSS_UserProfileObject attribute qtssUserExtendedRights
  qtssAttrRightQTSSExtended = 1 << 31  // This flag is reserved for future use by the server. Additional rights are stored in qtssUserQTSSExtendedRights.
};
typedef UInt32 QTSS_AttrRights; // see QTSS_UserProfileObject


/*
 * BUILT IN SERVER ATTRIBUTES
 *
 * The server maintains many attributes internally, and makes these available to plug-ins.
 * Each value is a standard attribute, with a name and everything. Plug-ins may resolve the id's of
 * these values by name if they'd like, but in the initialize role they will receive a struct of
 * all the ids of all the internally maintained server parameters. This enumerated type block defines the indexes
 * in that array for the id's.
 */

/**
 * @brief QTSS_RTPStreamObject parameters.
 *
 * All of these are preemptive safe.
 */
enum {

  /**
   * Unique ID identifying each stream. This will default to 0 unless set explicitly by a module.
   * @property r/w
   * @property UInt32
   */
  qtssRTPStrTrackID = 0,

  /**
   * SSRC (Synchronization Source) generated by the server. Guarenteed to be unique amongst all streams in the session.
   * @property read
   * @property UInt32
   */
  qtssRTPStrSSRC = 1,

  //This SSRC will be included in all RTCP Sender Reports generated by the server. See The RTP / RTCP RFC for more info on SSRCs.


  /**
   * Payload name of the media on this stream. This will be empty unless set explicitly by a module.
   * @property r/w
   * @property char array
   */
  qtssRTPStrPayloadName = 2,

  /**
   * Payload type of the media on this stream. This will default to qtssUnknownPayloadType unless set explicitly by a module.
   * @property r/w
   * @property QTSS_RTPPayloadType
   */
  qtssRTPStrPayloadType = 3,

  /**
   * Sequence number of the first RTP packet generated for this stream after the last PLAY request was issued.
   * If known, this must be set by a module before calling QTSS_Play. It is used by the server to generate a proper RTSP PLAY response.
   * @property r/w
   * @property SInt16
   */
  qtssRTPStrFirstSeqNumber = 4,

  /**
   * RTP timestamp of the first RTP packet generated for this stream after the last PLAY request was issued.
   * If known, this must be set by a module before calling QTSS_Play. It is used by the server to generate a proper RTSP PLAY response.
   * @property r/w
   * @property SInt32
   */
  qtssRTPStrFirstTimestamp = 5,

  /**
   * Timescale for the track. If known, this must be set before calling QTSS_Play.
   * @property r/w
   * @property SInt32
   */
  qtssRTPStrTimescale = 6,

  /**
   * Private
   * @property r/w
   * @property UInt32
   */
  qtssRTPStrQualityLevel = 7,

  /**
   * Private
   * @property r/w
   * @property UInt32
   */
  qtssRTPStrNumQualityLevels = 8,

  /**
   * Size of the client's buffer. Server always sets this to 3 seconds, it is up to a module to determine
   * what the buffer size is and set this attribute accordingly.
   * @property r/w
   * @property Float32
   */
  qtssRTPStrBufferDelayInSecs = 9,

  // All of these parameters come out of the last RTCP packet received on this stream.
  // If the corresponding field in the last RTCP packet was blank, the attribute value will be 0.


  /**
   * Fraction lost packets so far on this stream.
   * @property read
   * @property UInt32
   */
  qtssRTPStrFractionLostPackets = 10,

  /**
   * Total lost packets so far on this stream.
   * @property read
   * @property UInt32
   */
  qtssRTPStrTotalLostPackets = 11,

  /**
   * Cumulative jitter on this stream.
   * @property read
   * @property UInt32
   */
  qtssRTPStrJitter = 12,

  /**
   * Average bit rate received by the client in bits / sec.
   * @property read
   * @property UInt32
   */
  qtssRTPStrRecvBitRate = 13,

  /**
   * Average msec packets received late.
   * @property read
   * @property UInt16
   */
  qtssRTPStrAvgLateMilliseconds = 14,

  /**
   * Percent packets lost on this stream, as a fixed %.
   * @property read
   * @property UInt16
   */
  qtssRTPStrPercentPacketsLost = 15,

  /**
   * Average buffer delay in milliseconds
   * @property read
   * @property UInt16
   */
  qtssRTPStrAvgBufDelayInMsec = 16,

  /**
   * Non-zero if the client is reporting that the stream is getting better.
   * @property read
   * @property UInt16
   */
  qtssRTPStrGettingBetter = 17,

  /**
   * Non-zero if the client is reporting that the stream is getting worse.
   * @property read
   * @property UInt16
   */
  qtssRTPStrGettingWorse = 18,

  /**
   * Number of clients connected to this stream.
   * @property read
   * @property UInt32
   */
  qtssRTPStrNumEyes = 19,

  /**
   * Number of clients playing this stream.
   * @property read
   * @property UInt32
   */
  qtssRTPStrNumEyesActive = 20,

  /**
   * Number of clients connected but currently paused.
   * @property read
   * @property UInt32
   */
  qtssRTPStrNumEyesPaused = 21,

  /**
   * Total packets received by the client
   * @property read
   * @property UInt32
   */
  qtssRTPStrTotPacketsRecv = 22,

  /**
   * Total packets dropped by the client.
   * @property read
   * @property UInt16
   */
  qtssRTPStrTotPacketsDropped = 23,

  /**
   * Total packets lost.
   * @property read
   * @property UInt16
   */
  qtssRTPStrTotPacketsLost = 24,

  /**
   * How much the client buffer is filled in 10ths of a second.
   * @property read
   * @property UInt16
   */
  qtssRTPStrClientBufFill = 25,

  /**
   * Current frame rate, in frames per second.
   * @property read
   * @property UInt16
   */
  qtssRTPStrFrameRate = 26,

  /**
   * Expected frame rate, in frames per second.
   * @property read
   * @property UInt16
   */
  qtssRTPStrExpFrameRate = 27,

  /**
   * Number of times the audio has run dry.
   * @property read
   * @property UInt16
   */
  qtssRTPStrAudioDryCount = 28,

  //
  // Address & network related parameters

  /**
   * Is this RTP stream being sent over TCP? If false, it is being sent over UDP.
   * @property read
   * @property bool
   */
  qtssRTPStrIsTCP = 29,

  /**
   * A QTSS_StreamRef used for sending RTP or RTCP packets to the client.
   * Use the QTSS_WriteFlags to specify whether each packet is an RTP or RTCP packet.
   * @property read
   * @property QTSS_StreamRef
   */
  qtssRTPStrStreamRef = 30,

  /**
   * What kind of transport is being used?
   * @property read
   * @property QTSS_RTPTransportType
   */
  qtssRTPStrTransportType = 31,

  /**
   * Number of packets dropped by QTSS_Write because they were too old.
   * @property read
   * @property UInt32
   */
  qtssRTPStrStalePacketsDropped = 32,

  /**
   * Current ack timeout being advertised to the client in msec (part of reliable udp).
   * @property read
   * @property UInt32
   */
  qtssRTPStrCurrentAckTimeout = 33,


  /**
   * An RTCP delta count of lost packets equal to qtssRTPStrPercentPacketsLost
   * @property read
   * @property UInt32
   */
  qtssRTPStrCurPacketsLostInRTCPInterval = 34,

  /**
   * An RTCP delta count of packets
   * @property read
   * @property UInt32
   */
  qtssRTPStrPacketCountInRTCPInterval = 35,

  /**
   * Port the server is sending RTP packets from for this stream
   * @property read
   * @property UInt16
   */
  qtssRTPStrSvrRTPPort = 36,

  /**
   * Port the server is sending RTP packets to for this stream
   * @property read
   * @property UInt16
   */
  qtssRTPStrClientRTPPort = 37,

  /**
   * unicast or multicast
   * @property read
   * @property QTSS_RTPNetworkMode
   */
  qtssRTPStrNetworkMode = 38,


  /**
   * Stream thinning is disabled on this stream.
   * @property read
   * @property bool
   */
  qtssRTPStrThinningDisabled = 39,
  qtssRTPStrNumParams = 40

};
typedef UInt32 QTSS_RTPStreamAttributes;

/**
 * @brief QTSS_ClientSessionObject parameters.
 *
 * All of these are preemptive safe
 */
enum {

  /**
   * Iterated attribute. All the QTSS_RTPStreamRefs belonging to this session.
   * @property read
   * @property QTSS_RTPStreamObject
   */
  qtssCliSesStreamObjects = 0,

  /**
   * Time in milliseconds the session was created.
   * @property read
   * @property QTSS_TimeVal
   */
  qtssCliSesCreateTimeInMsec = 1,

  /**
   * Time in milliseconds the first QTSS_Play call was issued.
   * @property read
   * @property QTSS_TimeVal
   */
  qtssCliSesFirstPlayTimeInMsec = 2,

  /**
   * Time in milliseconds the most recent play was issued.
   * @property read
   * @property QTSS_TimeVal
   */
  qtssCliSesPlayTimeInMsec = 3,

  /**
   * Private - do not use
   * @property read
   * @property QTSS_TimeVal
   */
  qtssCliSesAdjustedPlayTimeInMsec = 4,

  /**
   * Number of RTP bytes sent so far on this session.
   * @property read
   * @property UInt32
   */
  qtssCliSesRTPBytesSent = 5,

  /**
   * Number of RTP packets sent so far on this session.
   * @property read
   * @property UInt32
   */
  qtssCliSesRTPPacketsSent = 6,

  /**
   * State of this session: is it paused or playing currently?
   * @property read
   * @property QTSS_RTPSessionState
   */
  qtssCliSesState = 7,

  /**
   * Presentation URL for this session. This URL is the "base" URL for the session. RTSP requests to this URL are assumed to affect all streams on the session.
   * @property read
   * @property char array
   */
  qtssCliSesPresentationURL = 8,

  /**
   * Private
   * @property read
   * @property char array
   */
  qtssCliSesFirstUserAgent = 9,

  /**
   * Duration of the movie for this session in seconds. This will default to 0 unless set by a module.
   * @property r/w
   * @property Float64
   */
  qtssCliSesMovieDurationInSecs = 10,

  /**
   * Movie size in bytes. This will default to 0 unless explictly set by a module
   * @property r/w
   * @property UInt64
   */
  qtssCliSesMovieSizeInBytes = 11,

  /**
   * average bits per second based on total RTP bits/movie duration. This will default to 0 unless explictly set by a module.
   * @property r/w
   * @property UInt32
   */
  qtssCliSesMovieAverageBitRate = 12,

  /**
   * Private
   * @property read
   * @property QTSS_RTSPSessionObject
   */
  qtssCliSesLastRTSPSession = 13,

  /**
   * full Presentation URL for this session. Same as qtssCliSesPresentationURL, but includes rtsp://domain.com prefix
   * @property read
   * @property char array
   */
  qtssCliSesFullURL = 14,

  /**
   * requestes host name for s session. Just the "domain.com" portion from qtssCliSesFullURL above
   * @property read
   * @property char array
   */
  qtssCliSesHostName = 15,


  /**
   * IP address addr of client, in dotted-decimal format.
   * @property read
   * @property char array
   */
  qtssCliRTSPSessRemoteAddrStr = 16,

  /**
   * DNS name of local IP address for this RTSP connection.
   * @property read
   * @property char array
   */
  qtssCliRTSPSessLocalDNS = 17,

  /**
   * Ditto, in dotted-decimal format.
   * @property read
   * @property char array
   */
  qtssCliRTSPSessLocalAddrStr = 18,


  /**
   * from the most recent (last) request.
   * @property read
   * @property char array
   */
  qtssCliRTSPSesUserName = 19,

  /**
   * from the most recent (last) request.
   * @property read
   * @property char array
   */
  qtssCliRTSPSesUserPassword = 20,

  /**
   * from the most recent (last) request.
   * @property read
   * @property char array
   */
  qtssCliRTSPSesURLRealm = 21,


  /**
   * Same as qtssRTSPReqRTSPReqRealStatusCode, the status from the most recent (last) request.
   * @property read
   * @property UInt32
   */
  qtssCliRTSPReqRealStatusCode = 22,

  /**
   * Must be set by a module that calls QTSS_Teardown if it is not a client requested disconnect.
   * @property r/w
   * @property QTSS_CliSesTeardownReason
   */
  qtssCliTeardownReason = 23,


  /**
   * Query string from the request that creates this  client session
   * @property read
   * @property char array
   */
  qtssCliSesReqQueryString = 24,


  /**
   * from the most recent (last) request. Error message sent back to client if response was an error.
   * @property read
   * @property char array
   */
  qtssCliRTSPReqRespMsg = 25,


  /**
   * Current bit rate of all the streams on this session. This is not an average. In bits per second.
   * @property read
   * @property UInt32
   */
  qtssCliSesCurrentBitRate = 26,

  /**
   * Current percent loss as a fraction. .5 = 50%. This is not an average.
   * @property read
   * @property Float32
   */
  qtssCliSesPacketLossPercent = 27,

  /**
   * Time in milliseconds that this client has been connected.
   * @property read
   * @property SInt64
   */
  qtssCliSesTimeConnectedInMsec = 28,

  /**
   * A unique, non-repeating ID for this session.
   * @property read
   * @property UInt32
   */
  qtssCliSesCounterID = 29,

  /**
   * The RTSP session ID that refers to this client session
   * @property read
   * @property char array
   */
  qtssCliSesRTSPSessionID = 30,

  /**
   * Modules can set this to be the number of frames skipped for this client
   * @property r/w
   * @property UInt32
   */
  qtssCliSesFramesSkipped = 31,

  /**
   * client session timeout in milliseconds refreshed by RefreshTimeout API call or any rtcp or rtp packet on the session.
   * @property r/w
   * @property UInt32
   */
  qtssCliSesTimeoutMsec = 32,

  /**
   * client overbuffers using dynamic rate streams
   * @property read
   * @property bool
   */
  qtssCliSesOverBufferEnabled = 33,

  /**
   * Number of RTCP packets received so far on this session.
   * @property read
   * @property UInt32
   */
  qtssCliSesRTCPPacketsRecv = 34,

  /**
   * Number of RTCP bytes received so far on this session.
   * @property read
   * @property UInt32
   */
  qtssCliSesRTCPBytesRecv = 35,

  /**
   * At least one of the streams in the session is thinned
   * @property read
   * @property bool
   */
  qtssCliSesStartedThinning = 36,


  /**
   * The last RTSP Bandwidth header value received from the client.
   * @property read
   * @property UInt32
   */
  qtssCliSessLastRTSPBandwidth = 37,

  qtssCliSesNumParams = 38
};
typedef UInt32 QTSS_ClientSessionAttributes;

/**
 * QTSS_RTSPSessionObject parameters
 */
enum {
  //  Valid in any role that receives a QTSS_RTSPSessionObject

  /**
   * This is a unique ID for each session since the server started up.
   * @property read
   * @property UInt32
   */
  qtssRTSPSesID = 0,

  /**
   * Local IP address for this RTSP connection
   * @property read
   * @property UInt32
   */
  qtssRTSPSesLocalAddr = 1,

  /**
   * Ditto, in dotted-decimal format.
   * @property read
   * @property char array
   */
  qtssRTSPSesLocalAddrStr = 2,

  /**
   * DNS name of local IP address for this RTSP connection.
   * @property read
   * @property char array
   */
  qtssRTSPSesLocalDNS = 3,

  /**
   * IP address of client.
   * @property read
   * @property UInt32
   */
  qtssRTSPSesRemoteAddr = 4,

  /**
   * IP address addr of client, in dotted-decimal format.
   * @property read
   * @property char array
   */
  qtssRTSPSesRemoteAddrStr = 5,

  /**
   * An event context for the RTSP connection to the client.
   * This should primarily be used to wait for EV_WR events if flow-controlled when responding to a client.
   * @property read
   * @property QTSS_EventContextRef
   */
  qtssRTSPSesEventCntxt = 6,

  /**
   * Is this a normal RTSP session, or is it a HTTP tunnelled RTSP session?
   * @property read
   * @property QTSS_RTSPSession
   */
  qtssRTSPSesType = 7,

  /**
   * A QTSS_StreamRef used for sending data to the RTSP client.
   * @property read
   * @property QTSS_RTSPSessionStream
   */
  qtssRTSPSesStreamRef = 8,


  /**
   * Private
   * @property read
   * @property char array
   */
  qtssRTSPSesLastUserName = 9,

  /**
   * Private
   * @property read
   * @property char array
   */
  qtssRTSPSesLastUserPassword = 10,

  /**
   * Private
   * @property read
   * @property char array
   */
  qtssRTSPSesLastURLRealm = 11,


  /**
   * This is the local port for the connection
   * @property read
   * @property UInt16
   */
  qtssRTSPSesLocalPort = 12,

  /**
   * This is the client port for the connection
   * @property read
   * @property UInt16
   */
  qtssRTSPSesRemotePort = 13,


  /**
   * Private
   * @property read
   * @property char array
   */
  qtssRTSPSesLastDigestChallenge = 14,
  qtssRTSPSesNumParams = 15
};
typedef UInt32 QTSS_RTSPSessionAttributes;

/**
 * Easy_HTTPSessionObject parameters
 */
enum {

  /**
   * This is a unique ID for each session since the server started up.
   * @property read
   * @property UInt32
   */
  easyHTTPSesID = 0,

  /**
   * Local IP address for this HTTP connection
   * @property read
   * @property UInt32
   */
  easyHTTPSesLocalAddr = 1,

  /**
   * Ditto, in dotted-decimal format.
   * @property read
   * @property char array
   */
  easyHTTPSesLocalAddrStr = 2,

  /**
   * DNS name of local IP address for this RTSP connection.
   * @property read
   * @property char array
   */
  easyHTTPSesLocalDNS = 3,

  /**
   * IP address of client.
   * @property read
   * @property UInt32
   */
  easyHTTPSesRemoteAddr = 4,

  /**
   * IP address addr of client, in dotted-decimal format.
   * @property read
   * @property char array
   */
  easyHTTPSesRemoteAddrStr = 5,

  /**
   * An event context for the HTTP connection to the client.
   * This should primarily be used to wait for EV_WR events if flow-controlled when responding to a client.
   * @property read
   * @property QTSS_EventContextRef
   */
  easyHTTPSesEventCntxt = 6,

  /**
   * Private
   * @property read
   * @property char array
   */
  easyHTTPSesLastUserName = 7,

  /**
   * Private
   * @property read
   * @property char array
   */
  easyHTTPSesLastUserPassword = 8,

  /**
   * Private
   * @property read
   * @property char array
   */
  easyHTTPSesLastURLRealm = 9,


  /**
   * This is the local port for the connection
   * @property read
   * @property UInt16
   */
  easyHTTPSesLocalPort = 10,

  /**
   * This is the client port for the connection
   * @property read
   * @property UInt16
   */
  easyHTTPSesRemotePort = 11,


  /**
   * Private
   * @property read
   * @property char array
   */
  easyHTTPSesLastToken = 12,

  /**
   * @property read
   * @property char array
   */
  easyHTTPSesContentBody = 13,

  /**
   * @property read
   * @property UInt32
   */
  easyHTTPSesContentBodyOffset = 14,

  easyHTTPSesNumParams = 15
};
typedef UInt32 Easy_HTTPSessionAttributes;

/**
 * @brief QTSS_RTSPRequestObject parameters.
 *
 * All text names are identical to the enumerated type names
 * All of these are pre-emptive safe parameters
 */
enum {
  //
  // Available in every role that receives the QTSS_RTSPRequestObject

  /**
   * The full request sent by the client
   * @property read
   * @property char array
   */
  qtssRTSPReqFullRequest = 0,

  //
  // Available in every method that receives the QTSS_RTSPRequestObject except for the QTSS_FilterMethod

  /**
   * RTSP Method of this request.
   * @property read
   * @property char array
   */
  qtssRTSPReqMethodStr = 1,

  /**
   * URI for this request, decode urlencode and converted to a local file system path.
   * @property r/w
   * @property char array
   * @note Not pre-emptive safe!!
   * @example /nice/live.sdp/trackID=0
   */
  qtssRTSPReqFilePath = 2,

  /**
   * raw URI for this request
   * @property read
   * @property char array
   * @example /nice/live.sdp/trackID=0
   */
  qtssRTSPReqURI = 3,

  /**
   * Same as qtssRTSPReqFilePath, without the last element of the path
   * @property read
   * @property char array
   * @note Not pre-emptive safe!!
   * @example /nice/live.sdp
   */
  qtssRTSPReqFilePathTrunc = 4,

  /**
   * Everything after the last path separator in the file system path
   * @property read
   * @property char array
   * @note Not pre-emptive safe!!
   * @example live.sdp
   */
  qtssRTSPReqFileName = 5,

  /**
   * If the URI ends with one or more digits, this points to those.
   * @property read
   * @property char array
   * @not Not pre-emptive safe!!
   */
  qtssRTSPReqFileDigit = 6,

  /**
   * The full URL, starting from "rtsp://"
   * @property read
   * @property char array
   * @example rtsp://www.easydarwin.org:554/nice/live.sdp/trackID=0
   */
  qtssRTSPReqAbsoluteURL = 7,

  /**
   * Absolute URL without last element of path
   * @property read
   * @property char array
   * @example rtsp://www.easydarwin.org:554/nice/live.sdp
   */
  qtssRTSPReqTruncAbsoluteURL = 8,

  /**
   * Method as QTSS_RTSPMethod
   * @property read
   * @property QTSS_RTSPMethod
   */
  qtssRTSPReqMethod = 9,

  /**
   * The current status code for the request as QTSS_RTSPStatusCode. By default, it is always qtssSuccessOK.
   * If a module sets this attribute, and calls QTSS_SendRTSPHeaders,
   * the status code of the header generated by the server will reflect this value.
   * @property r/w
   * @property QTSS_RTSPStatusCode
   */
  qtssRTSPReqStatusCode = 10,

  /**
   * Start time specified in Range: header of PLAY request.
   * @property read
   * @property Float64
   */
  qtssRTSPReqStartTime = 11,

  /**
   * Stop time specified in Range: header of PLAY request.
   * @property read
   * @property Float64
   */
  qtssRTSPReqStopTime = 12,

  /**
   * Will (should) the server keep the connection alive.
   * Set this to false if the connection should be terminated after completion of this request.
   * @property r/w
   * @property bool
   */
  qtssRTSPReqRespKeepAlive = 13,

  /**
   * Root directory to use for this request. The default value for this parameter is the server's media folder path.
   * Modules may set this attribute from the QTSS_RTSPRoute_Role.
   * @property r/w
   * @property char array
   * @note Not pre-emptive safe!!
   */
  qtssRTSPReqRootDir = 14,

  /**
   * Same as qtssRTSPReqStatusCode, but translated from QTSS_RTSPStatusCode into an actual RTSP status code.
   * @property read
   * @property UInt32
   */
  qtssRTSPReqRealStatusCode = 15,

  /**
   * A QTSS_StreamRef for sending data to the RTSP client. This stream ref,
   * unlike the one provided as an attribute in the QTSS_RTSPSessionObject,
   * will never return EWOULDBLOCK in response to a QTSS_Write or a QTSS_WriteV call.
   * @property read
   * @property QTSS_RTSPRequestStream
   */
  qtssRTSPReqStreamRef = 16,


  /**
   * decoded Authentication information when provided by the RTSP request.
   * @property read
   * @property char array
   * @see RTSPSessLastUserName
   */
  qtssRTSPReqUserName = 17,

  /**
   * decoded Authentication information when provided by the RTSP request.
   * @property read
   * @property char array
   * @see RTSPSessLastUserPassword
   */
  qtssRTSPReqUserPassword = 18,

  /**
   * Default is server pref based, set to false if request is denied.
   * Missing or bad movie files should allow the server to handle the situation and return true.
   * @property r/w
   * @property bool
   */
  qtssRTSPReqUserAllowed = 19,

  /**
   * The authorization entity for the client to display "Please enter password for -realm- at server name.
   * The default realm is "Streaming Server".
   * @property r/w
   * @property char array
   */
  qtssRTSPReqURLRealm = 20,

  /**
   * The full local path to the file.
   * This Attribute is first set after the Routing Role has run and before any other role is called.
   * @property read
   * @property char array
   * @note Not pre-emptive safe!!
   */
  qtssRTSPReqLocalPath = 21,

  /**
   *  If the RTSP request contains an If-Modified-Since header, this is the if-modified date, converted to a QTSS_TimeVal
   * @property read
   * @property QTSS_TimeVal
   */
  qtssRTSPReqIfModSinceDate = 22,

  /**
   * query stting (CGI parameters) passed to the server in the request URL, does not include the '?' separator
   * @property read
   * @property char array
   * @example channel=1&token=888888
   */
  qtssRTSPReqQueryString = 23,

  /**
   * A module sending an RTSP error to the client should set this to be a text message describing why the error occurred.
   * This description is useful to add to log files.
   * Once the RTSP response has been sent, this attribute contains the response message.
   * @property r/w
   * @property char array
   */
  qtssRTSPReqRespMsg = 24,

  /**
   *  Content length of incoming RTSP request body
   * @property read
   * @property UInt32
   */
  qtssRTSPReqContentLen = 25,

  /**
   *  Value of Speed header, converted to a Float32.
   * @property read
   * @property Float32
   */
  qtssRTSPReqSpeed = 26,

  /**
   *  Value of the late-tolerance field of the x-RTP-Options header, or -1 if not present.
   * @property read
   * @property Float32
   */
  qtssRTSPReqLateTolerance = 27,

  /**
   *  What kind of transport?
   * @property read
   * @property QTSS_RTPTransportType
   */
  qtssRTSPReqTransportType = 28,

  /**
   *  A setup request from the client. * maybe should just be an enum or the text of the mode value?
   * @property read
   * @property QTSS_RTPTransportMode
   */
  qtssRTSPReqTransportMode = 29,

  /**
   *  the ServerPort to respond to a client SETUP request with.
   * @property r/w
   * @property UInt16
   */
  qtssRTSPReqSetUpServerPort = 30,


  /**
   * Set by a module in the QTSS_RTSPSetAction_Role - for now, the server will set it as the role hasn't been added yet
   * @property r/w
   * @property QTSS_ActionFlags
   */
  qtssRTSPReqAction = 31,

  /**
   * Object's username is filled in by the server and its password and group memberships filled in by the authentication module.
   * @property r/w
   * @property QTSS_UserProfileObject
   */
  qtssRTSPReqUserProfile = 32,

  /**
   * The maxtime field of the x-Prebuffer RTSP header
   * @property read
   * @property Float32
   */
  qtssRTSPReqPrebufferMaxTime = 33,

  /**
   * @property read
   * @property QTSS_AuthScheme
   */
  qtssRTSPReqAuthScheme = 34,


  /**
   * Set by a module that wants the particular request to be
   * @property r/w
   * @property bool
   */
  qtssRTSPReqSkipAuthorization = 35,

  // allowed by all authorization modules


  /**
   * unicast or multicast
   * @property read
   * @property QTSS_RTPNetworkMode
   */
  qtssRTSPReqNetworkMode = 36,

  /**
   * -1 not in request, 0 off, 1 on
   * @property read
   * @property SInt32
   */
  qtssRTSPReqDynamicRateState = 37,


  /**
   * Value of the Bandwidth header. Default is 0.
   * @property read
   * @property UInt32
   */
  qtssRTSPReqBandwidthBits = 38,

  /**
   * Default is false, set to true if the user is found in the authenticate role
   * and the module wants to take ownership of authenticating the user.
   * @property r/w
   * @property bool
   */
  qtssRTSPReqUserFound = 39,

  /**
   * Default is false, set to true in the authorize role to take ownership of authorizing the request.
   * @property r/w
   * @property bool
   */
  qtssRTSPReqAuthHandled = 40,

  /**
   * Challenge used by the server for Digest authentication
   * @property read
   * @property char array
   */
  qtssRTSPReqDigestChallenge = 41,

  /**
   * Digest response used by the server for Digest authentication
   * @property read
   * @property char array
   */
  qtssRTSPReqDigestResponse = 42,
  qtssRTSPReqNumParams = 43

};
typedef UInt32 QTSS_RTSPRequestAttributes;

/**
 * QTSS_ServerObject parameters
 */
enum {
  // These parameters ARE pre-emptive safe.


  /**
   * The API version supported by this server (format 0xMMMMmmmm, where M=major version, m=minor version)
   * @property read
   * @property UInt32
   */
  qtssServerAPIVersion = 0,

  /**
   * The "default" DNS name of the server
   * @property read
   * @property char array
   */
  qtssSvrDefaultDNSName = 1,

  /**
   * The "default" IP address of the server
   * @property read
   * @property UInt32
   */
  qtssSvrDefaultIPAddr = 2,

  /**
   * Name of the server
   * @property read
   * @property char array
   */
  qtssSvrServerName = 3,

  /**
   * Version of the server
   * @property read
   * @property char array
   */
  qtssSvrServerVersion = 4,

  /**
   * When was the server built?
   * @property read
   * @property char array
   */
  qtssSvrServerBuildDate = 5,

  /**
   * Indexed parameter: all the ports the server is listening on
   * @property read
   * @property UInt16
   * @note NOT PREEMPTIVE SAFE!
   */
  qtssSvrRTSPPorts = 6,

  /**
   * Server: header that the server uses to respond to RTSP clients
   * @property read
   * @property char array
   */
  qtssSvrRTSPServerHeader = 7,

  // These parameters are NOT pre-emptive safe, they cannot be accessed
  // via. QTSS_GetValuePtr. Some exceptions noted below


  /**
   * The current state of the server.
   * If a module sets the server state, the server will respond in the appropriate fashion.
   * Setting to qtssRefusingConnectionsState causes the server to refuse connections,
   * setting to qtssFatalErrorState or qtssShuttingDownState causes the server to quit.
   * @property r/w
   * @property QTSS_ServerState
   */
  qtssSvrState = 8,

  /**
   * true if the server has run out of file descriptors, false otherwise
   * @property read
   * @property bool
   */
  qtssSvrIsOutOfDescriptors = 9,

  /**
   * Current number of connected clients over standard RTSP
   * @property read
   * @property UInt32
   */
  qtssRTSPCurrentSessionCount = 10,

  /**
   * Current number of connected clients over RTSP / HTTP
   * @property read
   * @property UInt32
   */
  qtssRTSPHTTPCurrentSessionCount = 11,


  /**
   * Number of UDP sockets currently being used by the server
   * @property read
   * @property UInt32
   */
  qtssRTPSvrNumUDPSockets = 12,

  /**
   * Number of clients currently connected to the server
   * @property read
   * @property UInt32
   */
  qtssRTPSvrCurConn = 13,

  /**
   * Total number of clients since startup
   * @property read
   * @property UInt32
   */
  qtssRTPSvrTotalConn = 14,

  /**
   * Current bandwidth being output by the server in bits per second
   * @property read
   * @property UInt32
   */
  qtssRTPSvrCurBandwidth = 15,

  /**
   * Total number of bytes served since startup
   * @property read
   * @property UInt64
   */
  qtssRTPSvrTotalBytes = 16,

  /**
   * Average bandwidth being output by the server in bits per second
   * @property read
   * @property UInt32
   */
  qtssRTPSvrAvgBandwidth = 17,

  /**
   * Current packets per second being output by the server
   * @property read
   * @property UInt32
   */
  qtssRTPSvrCurPackets = 18,

  /**
   * Total number of bytes served since startup
   * @property read
   * @property UInt64
   */
  qtssRTPSvrTotalPackets = 19,


  /**
   * The methods that the server supports.
   * Modules should append the methods they support to this attribute in their QTSS_Initialize_Role.
   * @property r/w
   * @property QTSS_RTSPMethod
   */
  qtssSvrHandledMethods = 20,

  /**
   * A module object representing each module
   * @property read
   * @property QTSS_ModuleObject
   * @note this IS PREMPTIVE SAFE!
   */
  qtssSvrModuleObjects = 21,

  /**
   * Time the server started up
   * @property read
   * @property QTSS_TimeVal
   */
  qtssSvrStartupTime = 22,

  /**
   * Server time zone (offset from GMT in hours)
   * @property read
   * @property SInt32
   */
  qtssSvrGMTOffsetInHrs = 23,

  /**
   * The "default" IP address of the server as a string
   * @property read
   * @property char array
   */
  qtssSvrDefaultIPAddrStr = 24,


  /**
   *  An object representing each the server's preferences
   * @property read
   * @property QTSS_PrefsObject
   */
  qtssSvrPreferences = 25,

  /**
   *  An object containing the server's error messages.
   * @property read
   * @property QTSS_Object
   */
  qtssSvrMessages = 26,

  /**
   * An object containing all client sessions stored as indexed QTSS_ClientSessionObject(s).
   * @property read
   * @property QTSS_Object
   */
  qtssSvrClientSessions = 27,

  /**
   * Server's current time in milliseconds.
   * Retrieving this attribute is equivalent to calling QTSS_Milliseconds
   * @property read
   * @property QTSS_TimeVal
   */
  qtssSvrCurrentTimeMilliseconds = 28,

  /**
   * Current % CPU being used by the server
   * @property read
   * @property Float32
   */
  qtssSvrCPULoadPercent = 29,


  /**
   * Number of buffers currently allocated for UDP retransmits
   * @property read
   * @property UInt32
   */
  qtssSvrNumReliableUDPBuffers = 30,

  /**
   * Amount of data in the reliable UDP buffers being wasted
   * @property read
   * @property UInt32
   */
  qtssSvrReliableUDPWastageInBytes = 31,

  /**
   * List of connected user sessions (updated by modules for their sessions)
   * @property r/w
   * @property QTSS_Object
   */
  qtssSvrConnectedUsers = 32,


  /**
   * build of the server
   * @property read
   * @property char array
   */
  qtssSvrServerBuild = 33,

  /**
   * Platform (OS) of the server
   * @property read
   * @property char array
   */
  qtssSvrServerPlatform = 34,

  /**
   * RTSP comment for the server header
   * @property read
   * @property char array
   */
  qtssSvrRTSPServerComment = 35,

  /**
   * Number of thinned sessions
   * @property read
   * @property SInt32
   */
  qtssSvrNumThinned = 36,

  /**
   * Number of task threads
   * @property read
   * @property UInt32
   * @see qtssPrefsRunNumThreads
   */
  qtssSvrNumThreads = 37,
  qtssSvrNumParams = 38
};
typedef UInt32 QTSS_ServerAttributes;

/**
 * QTSS_PrefsObject parameters
 */
enum {
  // Valid in all methods. None of these are pre-emptive safe, so the version
  // of QTSS_GetAttribute that copies data must be used.

  // All of these parameters are read-write.


  /**
   * RTSP timeout in seconds sent to the client.
   * @alias "rtsp_timeout"
   * @property UInt32
   */
  qtssPrefsRTSPTimeout = 0,

  /**
   * Amount of time in seconds the server will wait before disconnecting idle RTSP clients. 0 means no timeout
   * @alias "rtsp_session_timeout"
   * @property UInt32
   */
  qtssPrefsRTSPSessionTimeout = 1,

  /**
   * Amount of time in seconds the server will wait before disconnecting idle RTP clients. 0 means no timeout
   * @alias "rtp_session_timeout"
   * @property UInt32
   */
  qtssPrefsRTPSessionTimeout = 2,

  /**
   * Maximum # of concurrent RTP connections allowed by the server. -1 means unlimited.
   * @alias "maximum_connections"
   * @property SInt32
   */
  qtssPrefsMaximumConnections = 3,

  /**
   * Maximum amt of bandwidth the server is allowed to serve in K bits. -1 means unlimited.
   * @alias "maximum_bandwidth"
   * @property SInt32
   */
  qtssPrefsMaximumBandwidth = 4,

  /**
   * Path to the root movie folder
   * @alias "movie_folder"
   * @property char array
   */
  qtssPrefsMovieFolder = 5,

  /**
   * IP address the server should accept RTSP connections on. 0.0.0.0 means all addresses on the machine.
   * @alias "bind_ip_addr"
   * @property char array
   */
  qtssPrefsRTSPIPAddr = 6,

  /**
   * If true, the server will break in the debugger when an assert fails.
   * @alias "break_on_assert"
   * @property bool
   */
  qtssPrefsBreakOnAssert = 7,

  /**
   * If true, the server will automatically restart itself if it crashes.
   * @alias "auto_restart"
   * @property bool
   */
  qtssPrefsAutoRestart = 8,

  /**
   * Interval in seconds between updates of the server's total bytes and current bandwidth statistics
   * @alias "total_bytes_update"
   * @property UInt32
   */
  qtssPrefsTotalBytesUpdate = 9,

  /**
   * Interval in seconds between computations of the server's average bandwidth
   * @alias "average_bandwidth_update"
   * @property UInt32
   */
  qtssPrefsAvgBandwidthUpdate = 10,

  /**
   * Hard to explain... see streamingserver.conf
   * @alias "safe_play_duration"
   * @property UInt32
   */
  qtssPrefsSafePlayDuration = 11,

  /**
   * Path to the module folder
   * @alias "module_folder"
   * @property char array
   */
  qtssPrefsModuleFolder = 12,

  // There is a compiled-in error log module that loads before all the other modules
  // (so it can log errors from the get-go). It uses these prefs.


  /**
   * Name of error log file
   * @alias "error_logfile_name"
   * @property char array
   */
  qtssPrefsErrorLogName = 13,

  /**
   * Path to error log file directory
   * @alias "error_logfile_dir"
   * @property char array
   */
  qtssPrefsErrorLogDir = 14,

  /**
   * Interval in days between error logfile rolls
   * @alias "error_logfile_interval"
   * @property UInt32
   */
  qtssPrefsErrorRollInterval = 15,

  /**
   * Max size in bytes of the error log
   * @alias "error_logfile_size"
   * @property UInt32
   */
  qtssPrefsMaxErrorLogSize = 16,

  /**
   * Max verbosity level of messages the error logger will log
   * @alias "error_logfile_verbosity"
   * @property UInt32
   */
  qtssPrefsErrorLogVerbosity = 17,

  /**
   * Should the error logger echo messages to the screen?
   * @alias "screen_logging"
   * @property bool
   */
  qtssPrefsScreenLogging = 18,

  /**
   * Is error logging enabled?
   * @alias "error_logging"
   * @property bool
   */
  qtssPrefsErrorLogEnabled = 19,


  /**
   * Don't send video packets later than this
   * @alias "drop_all_video_delay"
   * @property SInt32
   */
  qtssPrefsDropVideoAllPacketsDelayInMsec = 20,

  /**
   * lateness at which we might start thinning
   * @alias "start_thinning_delay"
   * @property SInt32
   */
  qtssPrefsStartThinningDelayInMsec = 21,

  /**
   * default size that will be used for high bitrate movies
   * @alias "large_window_size"
   * @property UInt32
   */
  qtssPrefsLargeWindowSizeInK = 22,

  /**
   * bitrate at which we switch to larger window size
   * @alias "window_size_threshold"
   * @property UInt32
   */
  qtssPrefsWindowSizeThreshold = 23,


  /**
   * When streaming over TCP, this is the minimum size the TCP socket send buffer can be set to
   * @alias "min_tcp_buffer_size"
   * @property UInt32
   */
  qtssPrefsMinTCPBufferSizeInBytes = 24,

  /**
   * When streaming over TCP, this is the maximum size the TCP socket send buffer can be set to
   * @alias "max_tcp_buffer_size"
   * @property UInt32
   */
  qtssPrefsMaxTCPBufferSizeInBytes = 25,

  /**
   * When streaming over TCP, the size of the TCP send buffer is scaled based on the bitrate of the movie.
   * It will fit all the data that gets sent in this amount of time.
   * @alias "tcp_seconds_to_buffer"
   * @property Float32
   */
  qtssPrefsTCPSecondsToBuffer = 26,


  /**
   * when behind a round robin DNS, the client needs to be told the specific ip address of the maching handling its request.
   * this pref tells the server to repot its IP address in the reply to the HTTP GET request when tunneling RTSP through HTTP
   * @alias "do_report_http_connection_ip_address"
   * @property bool
   */
  qtssPrefsDoReportHTTPConnectionAddress = 27,


  /**
   * @alias "default_authorization_realm"
   * @property char array
   */
  qtssPrefsDefaultAuthorizationRealm = 28,//


  /**
   * Run under this user's account
   * @alias "run_user_name"
   * @property char array
   */
  qtssPrefsRunUserName = 29,

  /**
   * Run under this group's account
   * @alias "run_group_name"
   * @property char array
   */
  qtssPrefsRunGroupName = 30,


  /**
   * If true, the server will append the src address to the Transport header responses
   * @alias "append_source_addr_in_transport"
   * @property bool
   */
  qtssPrefsSrcAddrInTransport = 31,

  /**
   * @alias "rtsp_ports"
   * @property UInt16
   */
  qtssPrefsRTSPPorts = 32,


  /**
   * maximum interval between when a retransmit is supposed to be sent and when it actually gets sent.
   * Lower values means smoother flow but slower server performance
   * @alias "max_retransmit_delay"
   * @property UInt32
   */
  qtssPrefsMaxRetransDelayInMsec = 33,

  /**
   * default size that will be used for low bitrate movies
   * @alias "small_window_size"
   * @property UInt32
   */
  qtssPrefsSmallWindowSizeInK = 34,

  /**
   * Debugging only: turns on detailed logging of UDP acks / retransmits
   * @alias "ack_logging_enabled"
   * @property bool
   */
  qtssPrefsAckLoggingEnabled = 35,

  /**
   * interval (in Msec) between poll for RTCP packets
   * @alias "rtcp_poll_interval"
   * @property UInt32
   */
  qtssPrefsRTCPPollIntervalInMsec = 36,

  /**
   * Size of the receive socket buffer for udp sockets used to receive rtcp packets
   * @alias "rtcp_rcv_buf_size"
   * @property UInt32
   */
  qtssPrefsRTCPSockRcvBufSizeInK = 37,

  /**
   * @alias "send_interval"
   * @property UInt32
   */
  qtssPrefsSendInterval = 38,//

  /**
   * @alias "thick_all_the_way_delay"
   * @property UInt32
   */
  qtssPrefsThickAllTheWayDelayInMsec = 39,//

  /**
   * If empty, the server uses its own IP addr in the source= param of the transport header.
   * Otherwise, it uses this addr.
   * @alias "alt_transport_src_ipaddr"
   * @property char
   */
  qtssPrefsAltTransportIPAddr = 40,

  /**
   * This is the farthest in advance the server will send a packet to a client that supports overbuffering.
   * @alias "max_send_ahead_time"
   * @property UInt32
   */
  qtssPrefsMaxAdvanceSendTimeInSec = 41,

  /**
   * Is reliable UDP slow start enabled?
   * @alias "reliable_udp_slow_start"
   * @property bool
   */
  qtssPrefsReliableUDPSlowStart = 42,

  /**
   * @alias "enable_cloud_platform"
   * @property bool
   */
  qtssPrefsEnableCloudPlatform = 43,

  /**
   * Set this to be the authentication scheme you want the server to use. "basic", "digest", and "none" are the currently supported values
   * @alias "authentication_scheme"
   * @property char
   */
  qtssPrefsAuthenticationScheme = 44,

  /**
   * Feature rem
   * @alias "sdp_file_delete_interval_seconds"
   * @property UInt32
   */
  qtssPrefsDeleteSDPFilesInterval = 45,

  /**
   * If true, streaming server likes to be started at system startup
   * @alias "auto_start"
   * @property bool
   */
  qtssPrefsAutoStart = 46,

  /**
   * If true, uses reliable udp transport if requested by the client
   * @alias "reliable_udp"
   * @property bool
   */
  qtssPrefsReliableUDP = 47,

  /**
   * @alias "reliable_udp_dirs"
   * @property CharArray
   */
  qtssPrefsReliableUDPDirs = 48,

  /**
   * If enabled, server prints out interesting statistics for the reliable UDP clients
   * @alias "reliable_udp_printfs"
   * @property bool
   */
  qtssPrefsReliableUDPPrintfs = 49,


  /**
   * don't send any packets later than this
   * @alias "drop_all_packets_delay"
   * @property SInt32
   */
  qtssPrefsDropAllPacketsDelayInMsec = 50,

  /**
   * thin to key frames
   * @alias "thin_all_the_way_delay"
   * @property SInt32
   */
  qtssPrefsThinAllTheWayDelayInMsec = 51,

  /**
   * we always start to thin at this point
   * @alias "always_thin_delay"
   * @property SInt32
   */
  qtssPrefsAlwaysThinDelayInMsec = 52,

  /**
   * maybe start thicking at this point
   * @alias "start_thicking_delay"
   * @property SInt32
   */
  qtssPrefsStartThickingDelayInMsec = 53,

  /**
   * adjust thinnning params this often
   * @alias "quality_check_interval"
   * @property UInt32
   */
  qtssPrefsQualityCheckIntervalInMsec = 54,

  /**
   * Appends a content body string error message for reported RTSP errors.
   * @alias "RTSP_error_message"
   * @property bool
   *
   * @see QTSSModuleUtils::sEnableRTSPErrorMsg
   */
  qtssPrefsEnableRTSPErrorMessage = 55,

  /**
   * printfs incoming RTSPRequests and Outgoing RTSP responses.
   * @alias "RTSP_debug_printfs"
   * @property Boo1l6
   *
   * @see RTSPRequestStream::fPrintRTSP
   * @see RTSPResponseStream::fPrintRTSP
   */
  qtssPrefsEnableRTSPDebugPrintfs = 56,


  /**
   * write server stats to the monitor file
   * @alias "enable_monitor_stats_file"
   * @property bool
   */
  qtssPrefsEnableMonitorStatsFile = 57,

  /**
   * @alias "monitor_stats_file_interval_seconds"
   * @property private
   */
  qtssPrefsMonitorStatsFileIntervalSec = 58,

  /**
   * @alias "monitor_stats_file_name"
   * @property private
   */
  qtssPrefsMonitorStatsFileName = 59,


  /**
   * RTP and RTCP printfs of outgoing packets.
   * @alias "enable_packet_header_printfs"
   * @property bool
   */
  qtssPrefsEnablePacketHeaderPrintfs = 60,

  /**
   * set of printfs to print. Form is [text option] [;]  default is "rtp;rr;sr;". This means rtp packets, rtcp sender reports, and rtcp receiver reports.
   * @alias "packet_header_printf_options"
   * @property char
   */
  qtssPrefsPacketHeaderPrintfOptions = 61,

  /**
   * @alias "overbuffer_rate"
   * @property Float32
   */
  qtssPrefsOverbufferRate = 62,

  /**
   * default size that will be used for medium bitrate movies
   * @alias "medium_window_size"
   * @property UInt32
   */
  qtssPrefsMediumWindowSizeInK = 63,

  /**
   * bitrate at which we switch from medium to large window size
   * @alias "window_size_threshold"
   * @property UInt32
   */
  qtssPrefsWindowSizeMaxThreshold = 64,

  /**
   * Adds server info to the RTSP responses.
   * @alias "RTSP_server_info"
   * @property Boo1l6
   */
  qtssPrefsEnableRTSPServerInfo = 65,

  /**
   * if value is non-zero, will  create that many task threads; otherwise a thread will be created for each processor
   * @alias "run_num_threads"
   * @property UInt32
   */
  qtssPrefsRunNumThreads = 66,

  /**
   * path to pid file
   * @alias "pid_file"
   * @property Char Array
   */
  qtssPrefsPidFile = 67,

  /**
   * force log files to close after each write.
   * @alias "force_logs_close_on_write"
   * @property bool
   */
  qtssPrefsCloseLogsOnWrite = 68,

  /**
   * Usually used for performance testing. Turn off stream thinning from packet loss or stream lateness.
   * @alias "disable_thinning"
   * @property bool
   */
  qtssPrefsDisableThinning = 69,

  /**
   * name of player to match against the player's user agent header
   * @alias "player_requires_rtp_header_info"
   * @property Char array
   */
  qtssPrefsPlayersReqRTPHeader = 70,

  /**
   * name of player to match against the player's user agent header
   * @alias "player_requires_bandwidth_adjustment"
   * @property Char array
   */
  qtssPrefsPlayersReqBandAdjust = 71,

  /**
   * name of player to match against the player's user agent header
   * @alias "player_requires_no_pause_time_adjustment"
   * @property Char array
   */
  qtssPrefsPlayersReqNoPauseTimeAdjust = 72,


  /**
   * 0 is all day and best quality. Higher values are worse maximum depends on the media and the media module
   * @alias "default_stream_quality"
   * @property UInt16
   */
  qtssPrefsDefaultStreamQuality = 73,

  /**
   * name of players to match against the player's user agent header
   * @alias "player_requires_rtp_start_time_adjust"
   * @property Char Array
   */
  qtssPrefsPlayersReqRTPStartTimeAdjust = 74,


  /**
   * reflect all udp streams to the monitor ports, use an sdp to view
   * @alias "enable_udp_monitor_stream"
   * @property Boo1l6
   */
  qtssPrefsEnableUDPMonitor = 75,

  /**
   * localhost destination port of reflected stream
   * @alias "udp_monitor_video_port"
   * @property UInt16
   */
  qtssPrefsUDPMonitorAudioPort = 76,

  /**
   * localhost destination port of reflected stream
   * @alias "udp_monitor_audio_port"
   * @property UInt16
   */
  qtssPrefsUDPMonitorVideoPort = 77,

  /**
   * IP address the server should send RTP monitor reflected streams.
   * @alias "udp_monitor_dest_ip"
   * @property char array
   */
  qtssPrefsUDPMonitorDestIPAddr = 78,

  /**
   * client IP address the server monitor should reflect. *.*.*.* means all client addresses.
   * @alias "udp_monitor_src_ip"
   * @property char array
   */
  qtssPrefsUDPMonitorSourceIPAddr = 79,

  /**
   * server hint to access modules to allow guest access as the default (can be overriden in a qtaccess file or other means)
   * @alias "enable_allow_guest_authorize_default"
   * @property Boo1l6
   */
  qtssPrefsEnableAllowGuestDefault = 80,

  /**
   * if value is non-zero, the server will  create that many task threads; otherwise a single thread will be created.
   * @alias "run_num_rtsp_threads"
   * @property UInt32
   */
  qtssPrefsNumRTSPThreads = 81,


  /**
   * @alias "service_lan_port"
   * @property UInt16
   */
  easyPrefsHTTPServiceLanPort = 82,

  /**
   * @alias "service_wan_port"
   * @property UInt16
   */
  easyPrefsHTTPServiceWanPort = 83,


  /**
   * @alias "service_wan_ip"
   * @property char array
   */
  easyPrefsServiceWANIPAddr = 84,

  /**
   * @alias "rtsp_wan_port"
   * @property UInt16
   */
  easyPrefsRTSPWANPort = 85,


  /**
   * @alias "nginx_rtmp_server"
   * @property char array
   */
  easyPrefsNginxRTMPServer = 86,

  /**
   * @alias "service_open_ip"
   * @property Char array
   */
  edssPrefsServiceOpenIPAddrs = 87,

  qtssPrefsNumParams = 88
};

typedef UInt32 QTSS_PrefsAttributes;

enum {
  //QTSS_TextMessagesObject parameters

  // All of these parameters are read-only, char*'s, and preemptive-safe.

  qtssMsgNoMessage = 0,    //"NoMessage"
  qtssMsgNoURLInRequest = 1,
  qtssMsgBadRTSPMethod = 2,
  qtssMsgNoRTSPVersion = 3,
  qtssMsgNoRTSPInURL = 4,
  qtssMsgURLTooLong = 5,
  qtssMsgURLInBadFormat = 6,
  qtssMsgNoColonAfterHeader = 7,
  qtssMsgNoEOLAfterHeader = 8,
  qtssMsgRequestTooLong = 9,
  qtssMsgNoModuleFolder = 10,
  qtssMsgCouldntListen = 11,
  qtssMsgInitFailed = 12,
  qtssMsgNotConfiguredForIP = 13,
  qtssMsgDefaultRTSPAddrUnavail = 14,
  qtssMsgBadModule = 15,
  qtssMsgRegFailed = 16,
  qtssMsgRefusingConnections = 17,
  qtssMsgTooManyClients = 18,
  qtssMsgTooMuchThruput = 19,
  qtssMsgNoSessionID = 20,
  qtssMsgFileNameTooLong = 21,
  qtssMsgNoClientPortInTransport = 22,
  qtssMsgRTPPortMustBeEven = 23,
  qtssMsgRTCPPortMustBeOneBigger = 24,
  qtssMsgOutOfPorts = 25,
  qtssMsgNoModuleForRequest = 26,
  qtssMsgAltDestNotAllowed = 27,
  qtssMsgCantSetupMulticast = 28,
  qtssListenPortInUse = 29,
  qtssListenPortAccessDenied = 30,
  qtssListenPortError = 31,
  qtssMsgBadBase64 = 32,
  qtssMsgSomePortsFailed = 33,
  qtssMsgNoPortsSucceeded = 34,
  qtssMsgCannotCreatePidFile = 35,
  qtssMsgCannotSetRunUser = 36,
  qtssMsgCannotSetRunGroup = 37,
  qtssMsgNoSesIDOnDescribe = 38,
  qtssServerPrefMissing = 39,
  qtssServerPrefWrongType = 40,
  qtssMsgCantWriteFile = 41,
  qtssMsgSockBufSizesTooLarge = 42,
  qtssMsgBadFormat = 43,
  qtssMsgNumParams = 44

};
typedef UInt32 QTSS_TextMessagesAttributes;

enum {
  //QTSS_FileObject parameters

  // All of these parameters are preemptive-safe.


  /**
   * Stream ref for this file object
   * @property read
   * @property QTSS_FileStream
   */
  qtssFlObjStream = 0,

  /**
   * Name of the file system module handling this file object
   * @property read
   * @property char array
   */
  qtssFlObjFileSysModuleName = 1,

  /**
   * Length of the file
   * @property r/w
   * @property UInt64
   */
  qtssFlObjLength = 2,

  /**
   * Current position of the file pointer in the file.
   * @property read
   * @property UInt64
   */
  qtssFlObjPosition = 3,

  /**
   * Date & time of last modification
   * @property r/w
   * @property QTSS_TimeVal
   */
  qtssFlObjModDate = 4,

  qtssFlObjNumParams = 5
};
typedef UInt32 QTSS_FileObjectAttributes;

enum {
  //QTSS_ModuleObject parameters

  /**
   * Module name.
   * @property read
   * @property char array
   * @property preemptive-safe
   */
  qtssModName = 0,

  /**
   * Text description of what the module does
   * @property r/w
   * @property char array
   * @property not preemptive-safe
   */
  qtssModDesc = 1,

  /**
   * Version of the module. UInt32 format should be 0xMM.m.v.bbbb M=major version m=minor version v=very minor version b=build #
   * @property r/w
   * @property UInt32
   * @property not preemptive-safe
   */
  qtssModVersion = 2,

  /**
   * List of all the roles this module has registered for.
   * @property read
   * @property QTSS_Role
   * @property preemptive-safe
   */
  qtssModRoles = 3,

  /**
   * An object containing as attributes the preferences for this module
   * @property read
   * @property QTSS_ModulePrefsObject
   * @property preemptive-safe
   */
  qtssModPrefs = 4,

  /**
   * @property read
   * @property QTSS_Object
   * @property preemptive-safe
   */
  qtssModAttributes = 5,

  qtssModNumParams = 6
};
typedef UInt32 QTSS_ModuleObjectAttributes;

enum {
  //QTSS_AttrInfoObject parameters

  // All of these parameters are preemptive-safe.


  /**
   * Attribute name
   * @property read
   * @property char array
   */
  qtssAttrName = 0,

  /**
   * Attribute ID
   * @property read
   * @property QTSS_AttributeID
   */
  qtssAttrID = 1,

  /**
   * Data type
   * @property read
   * @property QTSS_AttrDataType
   */
  qtssAttrDataType = 2,

  /**
   * Permissions
   * @property read
   * @property QTSS_AttrPermission
   */
  qtssAttrPermissions = 3,

  qtssAttrInfoNumParams = 4
};
typedef UInt32 QTSS_AttrInfoObjectAttributes;

enum {
  //QTSS_UserProfileObject parameters

  // All of these parameters are preemptive-safe.

  qtssUserName = 0, //read  //char array
  qtssUserPassword = 1, //r/w   //char array
  qtssUserGroups = 2, //r/w   //char array -  multi-valued attribute, all values should be C strings padded with \0s to make them all of the same length
  qtssUserRealm = 3, //r/w   //char array -  the authentication realm for username
  qtssUserRights = 4, //r/w   //QTSS_AttrRights - rights granted this user
  qtssUserExtendedRights = 5, //r/w   //qtssAttrDataTypeCharArray - a list of strings with extended rights granted to the user.
  qtssUserQTSSExtendedRights = 6, //r/w   //qtssAttrDataTypeCharArray - a private list of strings with extended rights granted to the user and reserved by QTSS/Apple.
  qtssUserNumParams = 7,
};
typedef UInt32 QTSS_UserProfileObjectAttributes;

enum {
  //QTSS_ConnectedUserObject parameters

  //All of these are preemptive safe


  /**
   * type of user connection (e.g. "RTP reflected")
   * @property read
   * @property char array
   */
  qtssConnectionType = 0,

  /**
   * Time in milliseconds the session was created.
   * @property read
   * @property QTSS_TimeVal
   */
  qtssConnectionCreateTimeInMsec = 1,

  /**
   * Time in milliseconds the session was created.
   * @property read
   * @property QTSS_TimeVal
   */
  qtssConnectionTimeConnectedInMsec = 2,

  /**
   * Number of RTP bytes sent so far on this session.
   * @property read
   * @property UInt32
   */
  qtssConnectionBytesSent = 3,

  /**
   * Presentation URL for this session. This URL is the "base" URL for the session. RTSP requests to this URL are assumed to affect all streams on the session.
   * @property read
   * @property char array
   */
  qtssConnectionMountPoint = 4,

  /**
   * host name for this request
   * @property read
   * @property char array
   */
  qtssConnectionHostName = 5,


  /**
   * IP address addr of client, in dotted-decimal format.
   * @property read
   * @property char array
   */
  qtssConnectionSessRemoteAddrStr = 6,

  /**
   * Ditto, in dotted-decimal format.
   * @property read
   * @property char array
   */
  qtssConnectionSessLocalAddrStr = 7,


  /**
   * Current bit rate of all the streams on this session. This is not an average. In bits per second.
   * @property read
   * @property UInt32
   */
  qtssConnectionCurrentBitRate = 8,

  /**
   * Current percent loss as a fraction. .5 = 50%. This is not an average.
   * @property read
   * @property Float32
   */
  qtssConnectionPacketLossPercent = 9,


  /**
   * Internal, use qtssConnectionTimeConnectedInMsec above
   * @property read
   * @property QTSS_TimeVal
   */
  qtssConnectionTimeStorage = 10,

  qtssConnectionNumParams = 11
};
typedef UInt32 QTSS_ConnectedUserObjectAttributes;


/********************************************************************/
// QTSS API ROLES
//
// Each role represents a unique situation in which a module may be
// invoked. Modules must specify which roles they want to be invoked for. 

enum {
  //Global

  QTSS_Register_Role = FOUR_CHARS_TO_INT('r', 'e', 'g', ' '), //reg  //All modules get this once at startup
  QTSS_Initialize_Role = FOUR_CHARS_TO_INT('i', 'n', 'i', 't'), //init //Gets called once, later on in the startup process
  QTSS_Shutdown_Role = FOUR_CHARS_TO_INT('s', 'h', 'u', 't'), //shut //Gets called once at shutdown

  QTSS_ErrorLog_Role = FOUR_CHARS_TO_INT('e', 'l', 'o', 'g'), //elog //This gets called when the server wants to log an error.
  QTSS_RereadPrefs_Role = FOUR_CHARS_TO_INT('p', 'r', 'e', 'f'), //pref //This gets called when the server rereads preferences.
  QTSS_StateChange_Role = FOUR_CHARS_TO_INT('s', 't', 'a', 't'), //stat //This gets called whenever the server changes state.

  QTSS_Interval_Role = FOUR_CHARS_TO_INT('t', 'i', 'm', 'r'), //timr //This gets called whenever the module's interval timer times out calls.

  //RTSP-specific

  QTSS_RTSPFilter_Role = FOUR_CHARS_TO_INT('f', 'i', 'l', 't'), //filt //Filter all RTSP requests before the server parses them
  QTSS_RTSPRoute_Role = FOUR_CHARS_TO_INT('r', 'o', 'u', 't'), //rout //Route all RTSP requests to the correct root folder.
  QTSS_RTSPAuthenticate_Role = FOUR_CHARS_TO_INT('a', 't', 'h', 'n'), //athn //Authenticate the RTSP request username.
  QTSS_RTSPAuthorize_Role = FOUR_CHARS_TO_INT('a', 'u', 't', 'h'), //auth //Authorize RTSP requests to proceed
  QTSS_RTSPPreProcessor_Role = FOUR_CHARS_TO_INT('p', 'r', 'e', 'p'), //prep //Pre-process all RTSP requests before the server responds.

  //Modules may opt to "steal" the request and return a client response.

  QTSS_RTSPRequest_Role = FOUR_CHARS_TO_INT('r', 'e', 'q', 'u'), //requ //Process an RTSP request & send client response
  QTSS_RTSPPostProcessor_Role = FOUR_CHARS_TO_INT('p', 'o', 's', 't'), //post //Post-process all RTSP requests
  QTSS_RTSPSessionClosing_Role = FOUR_CHARS_TO_INT('s', 'e', 's', 'c'), //sesc //RTSP session is going away

  QTSS_RTSPIncomingData_Role = FOUR_CHARS_TO_INT('i', 'c', 'm', 'd'), //icmd //Incoming interleaved RTP data on this RTSP connection

  //RTP-specific

  QTSS_RTPSendPackets_Role = FOUR_CHARS_TO_INT('s', 'e', 'n', 'd'), //send //Send RTP packets to the client
  QTSS_ClientSessionClosing_Role = FOUR_CHARS_TO_INT('d', 'e', 's', 's'), //dess //Client session is going away

  //RTCP-specific

  QTSS_RTCPProcess_Role = FOUR_CHARS_TO_INT('r', 't', 'c', 'p'), //rtcp //Process all RTCP packets sent to the server

  //File system roles

  QTSS_OpenFilePreProcess_Role = FOUR_CHARS_TO_INT('o', 'p', 'p', 'r'),  //oppr
  QTSS_OpenFile_Role = FOUR_CHARS_TO_INT('o', 'p', 'f', 'l'),  //opfl
  QTSS_AdviseFile_Role = FOUR_CHARS_TO_INT('a', 'd', 'f', 'l'),  //adfl
  QTSS_ReadFile_Role = FOUR_CHARS_TO_INT('r', 'd', 'f', 'l'),  //rdfl
  QTSS_CloseFile_Role = FOUR_CHARS_TO_INT('c', 'l', 'f', 'l'),  //clfl
  QTSS_RequestEventFile_Role = FOUR_CHARS_TO_INT('r', 'e', 'f', 'l'),  //refl

  //EasyHLSModule

  Easy_HLSOpen_Role = FOUR_CHARS_TO_INT('h', 'l', 's', 'o'),  //hlso
  Easy_HLSClose_Role = FOUR_CHARS_TO_INT('h', 'l', 's', 'c'),  //hlsc

  //EasyCMSModule

  Easy_CMSFreeStream_Role = FOUR_CHARS_TO_INT('e', 'f', 's', 'r'),  //efsr

  //EasyRedisModule

  Easy_RedisSetRTSPLoad_Role = FOUR_CHARS_TO_INT('c', 'r', 'n', 'r'),    //crnr
  Easy_RedisUpdateStreamInfo_Role = FOUR_CHARS_TO_INT('a', 'p', 'n', 'r'),    //apnr
  Easy_RedisTTL_Role = FOUR_CHARS_TO_INT('t', 't', 'l', 'r'),    //ttlr
  Easy_RedisGetAssociatedCMS_Role = FOUR_CHARS_TO_INT('g', 'a', 'c', 'r'),    //gacr
  Easy_RedisJudgeStreamID_Role = FOUR_CHARS_TO_INT('j', 's', 'i', 'r'),    //jsir

  //RESTful

  Easy_GetDeviceStream_Role = FOUR_CHARS_TO_INT('g', 'd', 's', 'r'),    //gdsr
  Easy_LiveDeviceStream_Role = FOUR_CHARS_TO_INT('l', 'd', 's', 'r'),    //ldsr
};
typedef UInt32 QTSS_Role;


//***********************************************/
// TYPEDEFS

typedef void *QTSS_StreamRef;
typedef void *QTSS_Object;
typedef void *QTSS_ServiceFunctionArgsPtr;
typedef SInt32 QTSS_AttributeID;
typedef SInt32 QTSS_ServiceID;
typedef SInt64 QTSS_TimeVal;

typedef QTSS_Object QTSS_RTPStreamObject;
typedef QTSS_Object QTSS_RTSPSessionObject;
typedef QTSS_Object QTSS_RTSPRequestObject;
typedef QTSS_Object QTSS_RTSPHeaderObject;
typedef QTSS_Object QTSS_ClientSessionObject;
typedef QTSS_Object QTSS_ServerObject;
typedef QTSS_Object QTSS_PrefsObject;
typedef QTSS_Object QTSS_TextMessagesObject;
typedef QTSS_Object QTSS_FileObject;
typedef QTSS_Object QTSS_ModuleObject;
typedef QTSS_Object QTSS_ModulePrefsObject;
typedef QTSS_Object QTSS_AttrInfoObject;
typedef QTSS_Object QTSS_UserProfileObject;
typedef QTSS_Object QTSS_ConnectedUserObject;

typedef QTSS_StreamRef QTSS_ErrorLogStream;
typedef QTSS_StreamRef QTSS_FileStream;
typedef QTSS_StreamRef QTSS_RTSPSessionStream;
typedef QTSS_StreamRef QTSS_RTSPRequestStream;
typedef QTSS_StreamRef QTSS_RTPStreamStream;
typedef QTSS_StreamRef QTSS_SocketStream;

typedef QTSS_RTSPStatusCode QTSS_SessionStatusCode;

//***********************************************/
// ROLE PARAMETER BLOCKS
//
// Each role has a unique set of parameters that get passed
// to the module.

typedef struct {
  char outModuleName[QTSS_MAX_MODULE_NAME_LENGTH];
} QTSS_Register_Params;

typedef struct {
  QTSS_ServerObject inServer;           // Global dictionaries
  QTSS_PrefsObject inPrefs;
  QTSS_TextMessagesObject inMessages;
  QTSS_ErrorLogStream inErrorLogStream; // Writing to this stream causes modules to be invoked in the QTSS_ErrorLog_Role
  QTSS_ModuleObject inModule;
} QTSS_Initialize_Params;

typedef struct {
  QTSS_ErrorVerbosity inVerbosity;
  char *inBuffer;

} QTSS_ErrorLog_Params;

typedef struct {
  QTSS_ServerState inNewState;
} QTSS_StateChange_Params;

typedef struct {
  QTSS_RTSPSessionObject inRTSPSession;
  QTSS_RTSPRequestObject inRTSPRequest;
  QTSS_RTSPHeaderObject inRTSPHeaders;
  QTSS_ClientSessionObject inClientSession;

} QTSS_StandardRTSP_Params;

typedef struct {
  QTSS_RTSPSessionObject inRTSPSession;
  QTSS_RTSPRequestObject inRTSPRequest;
  char **outNewRequest;

} QTSS_Filter_Params;

typedef struct {
  QTSS_RTSPRequestObject inRTSPRequest;
} QTSS_RTSPAuth_Params;

typedef struct {
  QTSS_RTSPSessionObject inRTSPSession;
  QTSS_ClientSessionObject inClientSession;
  char *inPacketData;
  UInt32 inPacketLen;

} QTSS_IncomingData_Params;

typedef struct {
  QTSS_RTSPSessionObject inRTSPSession;
} QTSS_RTSPSession_Params;

typedef struct {
  QTSS_ClientSessionObject inClientSession;
  QTSS_TimeVal inCurrentTime;
  QTSS_TimeVal outNextPacketTime;
} QTSS_RTPSendPackets_Params;

typedef struct {
  QTSS_ClientSessionObject inClientSession;
  QTSS_CliSesClosingReason inReason;
} QTSS_ClientSessionClosing_Params;

typedef struct {
  QTSS_ClientSessionObject inClientSession;
  QTSS_RTPStreamObject inRTPStream;
  void *inRTCPPacketData;
  UInt32 inRTCPPacketDataLen;
} QTSS_RTCPProcess_Params;

typedef struct {
  char *inPath;
  QTSS_OpenFileFlags inFlags;
  QTSS_Object inFileObject;
} QTSS_OpenFile_Params;

typedef struct {
  QTSS_Object inFileObject;
  UInt64 inPosition;
  UInt32 inSize;
} QTSS_AdviseFile_Params;

typedef struct {
  QTSS_Object inFileObject;
  UInt64 inFilePosition;
  void *ioBuffer;
  UInt32 inBufLen;
  UInt32 *outLenRead;
} QTSS_ReadFile_Params;

typedef struct {
  QTSS_Object inFileObject;
} QTSS_CloseFile_Params;

typedef struct {
  QTSS_Object inFileObject;
  QTSS_EventType inEventMask;
} QTSS_RequestEventFile_Params;

typedef struct {
  char *inDevice;
  UInt32 inChannel;
  CF::Net::EasyStreamType inStreamType;
  char *outUrl;
  bool outIsReady;
} Easy_GetDeviceStream_Params;

//redis module
typedef struct {
  char *inStreamName;
  UInt32 inChannel;
  UInt32 inNumOutputs;
  UInt32 inBitrate;
  Easy_RedisAction inAction;
} Easy_StreamInfo_Params;

typedef struct {
  char *inSerial;
  char *outCMSIP;
  char *outCMSPort;
} QTSS_GetAssociatedCMS_Params;

typedef struct {
  char *inStreanID;
  char *outresult;
} QTSS_JudgeStreamID_Params;

typedef union {
  QTSS_Register_Params regParams;
  QTSS_Initialize_Params initParams;
  QTSS_ErrorLog_Params errorParams;
  QTSS_StateChange_Params stateChangeParams;

  QTSS_Filter_Params rtspFilterParams;
  QTSS_IncomingData_Params rtspIncomingDataParams;
  QTSS_StandardRTSP_Params rtspRouteParams;
  QTSS_RTSPAuth_Params rtspAthnParams;
  QTSS_StandardRTSP_Params rtspAuthParams;
  QTSS_StandardRTSP_Params rtspPreProcessorParams;
  QTSS_StandardRTSP_Params rtspRequestParams;
  QTSS_StandardRTSP_Params rtspPostProcessorParams;
  QTSS_RTSPSession_Params rtspSessionClosingParams;

  QTSS_RTPSendPackets_Params rtpSendPacketsParams;
  QTSS_ClientSessionClosing_Params clientSessionClosingParams;
  QTSS_RTCPProcess_Params rtcpProcessParams;

  QTSS_OpenFile_Params openFilePreProcessParams;
  QTSS_OpenFile_Params openFileParams;
  QTSS_AdviseFile_Params adviseFileParams;
  QTSS_ReadFile_Params readFileParams;
  QTSS_CloseFile_Params closeFileParams;
  QTSS_RequestEventFile_Params reqEventFileParams;

  Easy_StreamInfo_Params easyStreamInfoParams;
  QTSS_GetAssociatedCMS_Params GetAssociatedCMSParams;
  QTSS_JudgeStreamID_Params JudgeStreamIDParams;

  Easy_GetDeviceStream_Params easyGetDeviceStreamParams;

} QTSS_RoleParams, *QTSS_RoleParamPtr;

typedef struct {
  void *packetData;
  QTSS_TimeVal packetTransmitTime;
  QTSS_TimeVal suggestedWakeupTime;
} QTSS_PacketStruct;


/********************************************************************/
// ENTRYPOINTS & FUNCTION TYPEDEFS

// MAIN ENTRYPOINT FOR MODULES
//
// Every QTSS API must implement two functions: a main entrypoint, and a dispatch
// function. The main entrypoint gets called by the server at startup to do some
// initialization. Your main entrypoint must follow the convention established below
//
// QTSS_Error mymodule_main(void* inPrivateArgs)
// {
//      return _stublibrary_main(inPrivateArgs, MyDispatchFunction);
// }
//
//

typedef QTSS_Error (*QTSS_MainEntryPointPtr)(void *inPrivateArgs);
typedef QTSS_Error (*QTSS_DispatchFuncPtr)(QTSS_Role inRole,
                                           QTSS_RoleParamPtr inParamBlock);

// STUB LIBRARY MAIN
QTSS_Error _stublibrary_main(void *inPrivateArgs, QTSS_DispatchFuncPtr inDispatchFunc);

/********************************************************************/
//  QTSS_New
//  QTSS_Delete
//
//  These should be used for all dynamic memory allocation done from
//  within modules. The memoryIdentifier is used for debugging:
//  the server can track this memory to make memory leak debugging easier.
void *QTSS_New(FourCharCode inMemoryIdentifier, UInt32 inSize);
void QTSS_Delete(void *inMemory);

/********************************************************************/
//  QTSS_Milliseconds
//
//  The server maintains a millisecond timer internally. The current
//  value of that timer can be obtained from this function. This value
//  is not valid between server executions.
//
//  All millisecond values used in QTSS API use this timer, unless otherwise noted
QTSS_TimeVal QTSS_Milliseconds();


/********************************************************************/
//  QTSS_MilliSecsTo1970Secs
//
//  Convert milliseconds from the QTSS_Milliseconds call to 
//  second's since 1970
//
time_t QTSS_MilliSecsTo1970Secs(QTSS_TimeVal inQTSS_MilliSeconds);

/********************************************************************/
//  QTSS_AddRole
//
//  Only available from QTSS_Initialize role. Call this for all the roles you
//  would like your module to operate on.
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_RequestFailed:     If module is registering for the QTSS_RTSPRequest_Role
//                                      and there already is such a module.
//              QTSS_BadArgument:   Registering for a nonexistent role.
QTSS_Error QTSS_AddRole(QTSS_Role inRole);


/*****************************************/
//  ATTRIBUTE / OBJECT CALLBACKS
//


/********************************************************************/
//  QTSS_LockObject
//
//  Grabs the mutex for this object so that accesses to the objects attributes
//  from other threads will block.  Note that objects created through QTSS_CreateObjectValue
//  will share a mutex with the parent object.
//
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument:   bad object
QTSS_Error QTSS_LockObject(QTSS_Object inObject);

/********************************************************************/
//  QTSS_UnlockObject
//
//  Releases the mutex for this object.
//
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument:   bad object
QTSS_Error QTSS_UnlockObject(QTSS_Object inObject);

/********************************************************************/
//  QTSS_CreateObjectType
//
//  Creates a new object type.  Attributes can be added to this object type and then it can
//  be passed into QTSS_CreateObjectValue.
//
//  This may only be called from the QTSS_Register role.
//
//  Returns:    QTSS_NoErr
//              QTSS_RequestFailed: Too many object types already exist.
QTSS_Error QTSS_CreateObjectType(QTSS_ObjectType *outType);

/********************************************************************/
//  QTSS_AddStaticAttribute
//
//  Adds a new static attribute to a predefined object type. All added attributes implicitly have
//  qtssAttrModeRead, qtssAttrModeWrite, and qtssAttrModePreempSafe permissions. "inUnused" should
//  always be NULL. Specify the data type and name of the attribute.
//
//  This may only be called from the QTSS_Register role.
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Adding an attribute to a nonexistent object type, attribute
//                      name too long, or NULL arguments.
//              QTSS_AttrNameExists: The name must be unique.
QTSS_Error QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, char const *inAttrName, void *inUnused, QTSS_AttrDataType inAttrDataType);

/********************************************************************/
//  QTSS_AddInstanceAttribute
//
//  Adds a new instance attribute to a predefined object type. All added attributes implicitly have
//  qtssAttrModeRead, qtssAttrModeWrite, and qtssAttrModePreempSafe permissions. "inUnused" should
//  always be NULL. Specify the data type and name of the attribute.
//
//  This may be called at any time.
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Adding an attribute to a nonexistent object type, attribute
//                      name too long, or NULL arguments.
//              QTSS_AttrNameExists: The name must be unique.
QTSS_Error QTSS_AddInstanceAttribute(QTSS_Object inObject, char const *inAttrName, void *inUnused, QTSS_AttrDataType inAttrDataType);

/********************************************************************/
//  QTSS_RemoveInstanceAttribute
//
//  Removes an existing instance attribute. This may be called at any time
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Bad object type.
//              QTSS_AttrDoesntExist: Bad attribute ID
QTSS_Error QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID);

/********************************************************************/
//  Getting attribute information
//
//  The following callbacks allow modules to discover at runtime what
//  attributes exist in which objects and object types, and discover
//  all attribute meta-data

/********************************************************************/
//  QTSS_IDForAttr
//
//  Given an attribute name, this returns its accompanying attribute ID.
//  The ID can in turn be used to retrieve the attribute value from
//  a object. This callback applies only to static attributes 
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_IDForAttr(QTSS_ObjectType inObjectType, char const *inAttributeName, QTSS_AttributeID *outID);

/********************************************************************/
//  QTSS_GetAttrInfoByID
//
//  Searches for an attribute with the specified ID in the specified object.
//  If found, this function returns a QTSS_AttrInfoObject describing the attribute.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument
//              QTSS_AttrDoesntExist
QTSS_Error QTSS_GetAttrInfoByID(QTSS_Object inObject, QTSS_AttributeID inAttrID, QTSS_AttrInfoObject *outAttrInfoObject);

/********************************************************************/
//  QTSS_GetAttrInfoByName
//
//  Searches for an attribute with the specified name in the specified object.
//  If found, this function returns a QTSS_AttrInfoObject describing the attribute.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument
//              QTSS_AttrDoesntExist
QTSS_Error QTSS_GetAttrInfoByName(QTSS_Object inObject, char *inAttrName, QTSS_AttrInfoObject *outAttrInfoObject);

/********************************************************************/
//  QTSS_GetAttrInfoByIndex
//
//  Allows caller to iterate over all the attributes in the specified object.
//  Returns a QTSS_AttrInfoObject for the attribute with the given index (0.. num attributes).
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument
//              QTSS_AttrDoesntExist
QTSS_Error QTSS_GetAttrInfoByIndex(QTSS_Object inObject, UInt32 inIndex, QTSS_AttrInfoObject *outAttrInfoObject);

/********************************************************************/
//  QTSS_GetNumAttributes
//
//  Returns the number of attributes in the specified object.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//
QTSS_Error QTSS_GetNumAttributes(QTSS_Object inObject, UInt32 *outNumAttributes);

/**
 * @note NOT TO BE USED WITH NON-PREEMPTIVE-SAFE attributes (or provide your own locking using QTSS_LockObject).
 * @return QTSS_NoErr
 * @return QTSS_BadArgument: Bad argument
 * @return QTSS_NotPreemptiveSafe: Attempt to get a non-preemptive safe attribute
 * @return QTSS_BadIndex: Attempt to get non-existent index.
 */
QTSS_Error QTSS_GetValuePtr(QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex, void **outBuffer, UInt32 *outLen);

/********************************************************************/
//  QTSS_GetValue
//
//  Copies the data into provided buffer. If QTSS_NotEnoughSpace is returned, outLen is still set.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_NotEnoughSpace: Value is too big for buffer provided.
//              QTSS_BadIndex: Attempt to get non-existent index.
QTSS_Error QTSS_GetValue(QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex, void *ioBuffer, UInt32 *ioLen);

/********************************************************************/
//  QTSS_GetValueAsString
//
//  Returns the specified attribute converted to a C-string. This call allocates
//  memory for the string which should be disposed of using QTSS_Delete.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_BadIndex: Attempt to get non-existent index.
QTSS_Error QTSS_GetValueAsString(QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex, char **outString);


/********************************************************************/
//  QTSS_TypeStringToType
//  QTSS_TypeToTypeString
//
//  Returns a text name for the specified QTSS_AttrDataType, or vice-versa
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument
QTSS_Error QTSS_TypeStringToType(char const *inTypeString, QTSS_AttrDataType *outType);
QTSS_Error QTSS_TypeToTypeString(const QTSS_AttrDataType inType, char **outTypeString);


/********************************************************************/
//  QTSS_StringToValue
//
//  Given a C-string and a QTSS_AttrDataType, this function converts the C-string
//  to the specified type and puts the result in ioBuffer. ioBuffer must be allocated
//  by the caller and must be big enough to contain the converted value.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_NotEnoughSpace: Value is too big for buffer provided.
//
//  QTSS_ValueToString
//
//  Given a buffer containing a value of the specified type, this function converts
//  the value to a C-string. This string is allocated internally and must be disposed of
//  using QTSS_Delete
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_StringToValue(char const *inValueAsString, const QTSS_AttrDataType inType, void *ioBuffer, UInt32 *ioBufSize);
QTSS_Error QTSS_ValueToString(const void *inValue, const UInt32 inValueLen, const QTSS_AttrDataType inType, char **outString);

/**
 * @return QTSS_NoErr
 * @return QTSS_BadArgument: Bad argument
 * @return QTSS_ReadOnly: Attribute is read only.
 * @return QTSS_BadIndex: Attempt to set non-0 index of attribute with a param retrieval function.
 */
QTSS_Error QTSS_SetValue(QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex, const void *inBuffer, UInt32 inLen);

/**
 * This allows you to have an attribute that simply reflects the value of a variable in your module.
 * If the update to this variable is not atomic, you should protect updates using QTSS_LockObject.
 * This can't be used with indexed attributes. Make sure the inBuffer provided exists as long as this
 * attribute exists.
 *
 * @return QTSS_NoErr
 * @return QTSS_BadArgument: Bad argument
 * @return QTSS_ReadOnly: Attribute is read only.
 */
QTSS_Error QTSS_SetValuePtr(QTSS_Object inObject, QTSS_AttributeID inID, const void *inBuffer, UInt32 inLen);

/********************************************************************/
//  QTSS_CreateObjectValue
//
//  Returns:    QTSS_NoErr
//                              QTSS_BadArgument: Bad argument
//                              QTSS_ReadOnly: Attribute is read only.
//
QTSS_Error QTSS_CreateObjectValue(QTSS_Object inObject, QTSS_AttributeID inID, QTSS_ObjectType inType, UInt32 *outIndex, QTSS_Object *outCreatedObject);

/********************************************************************/
//  QTSS_GetNumValues
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//
QTSS_Error QTSS_GetNumValues(QTSS_Object inObject, QTSS_AttributeID inID, UInt32 *outNumValues);

/********************************************************************/
//  QTSS_RemoveValue
//
//  This function removes the value with the specified index. If there
//  are any values following this index, they will be reordered.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_ReadOnly: Attribute is read only.
//              QTSS_BadIndex: Attempt to set non-0 index of attribute with a param retrieval function.
//
QTSS_Error QTSS_RemoveValue(QTSS_Object inObject, QTSS_AttributeID inID, UInt32 inIndex);

/*****************************************/
//  STREAM CALLBACKS
//
//  The QTSS API provides QTSS_StreamRefs as a generalized stream abstraction. Mostly,
//  QTSS_StreamRefs are used for communicating with the client. For instance,
//  in the QTSS_RTSPRequest_Role, modules receive a QTSS_StreamRef which can be
//  used for reading RTSP data from the client, and sending RTSP response data to the client.
//
//  Additionally, QTSS_StreamRefs are generalized enough to be used in many other situations.
//  For instance, modules receive a QTSS_StreamRef for the error log. When modules want
//  to report errors, they can use these same routines, passing in the error log StreamRef.

/********************************************************************/
//  QTSS_Write
//
//  Writes data to a stream.
//
//  Returns:    QTSS_NoErr
//              QTSS_WouldBlock: The stream cannot accept any data at this time.
//              QTSS_NotConnected: The stream receiver is no longer connected.
//              QTSS_BadArgument:   NULL argument.
QTSS_Error QTSS_Write(QTSS_StreamRef inRef, const void *inBuffer, UInt32 inLen, UInt32 *outLenWritten, QTSS_WriteFlags inFlags);

/********************************************************************/
//  QTSS_WriteV
//
//  Works similar to the POSIX WriteV, and takes a POSIX iovec.
//  THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
//
//  Returns:    QTSS_NoErr
//              QTSS_WouldBlock: The stream cannot accept any data at this time.
//              QTSS_NotConnected: The stream receiver is no longer connected.
//              QTSS_BadArgument:   NULL argument.
QTSS_Error QTSS_WriteV(QTSS_StreamRef inRef, iovec *inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32 *outLenWritten);

/********************************************************************/
//  QTSS_Flush
//
//  Some QTSS_StreamRefs (QTSS_RequestRef, for example) buffers data before sending it
//  out. Calling this forces the stream to write the data immediately.
//
//  Returns:    QTSS_NoErr
//              QTSS_WouldBlock: Stream cannot be completely flushed at this time.
//              QTSS_NotConnected: The stream receiver is no longer connected.
//              QTSS_BadArgument:   NULL argument.
QTSS_Error QTSS_Flush(QTSS_StreamRef inRef);

/********************************************************************/
//  QTSS_Read
//
//  Reads data out of the stream
//
//  Arguments   inRef:      The stream to read from.
//              ioBuffer:   A buffer to place the read data
//              inBufLen:   The length of ioBuffer.
//              outLengthRead:  If function returns QTSS_NoErr, on output this will be set to the
//                              amount of data actually read.
//
//  Returns:    QTSS_NoErr
//              QTSS_WouldBlock
//              QTSS_RequestFailed
//              QTSS_BadArgument
QTSS_Error QTSS_Read(QTSS_StreamRef inRef, void *ioBuffer, UInt32 inBufLen, UInt32 *outLengthRead);

/********************************************************************/
//  QTSS_Seek
//
//  Sets the current stream position to inNewPosition
//
//  Arguments   inRef:      The stream to read from.
//              inNewPosition:  Offset from the start of the stream.
//
//  Returns:    QTSS_NoErr
//              QTSS_RequestFailed
//              QTSS_BadArgument
QTSS_Error QTSS_Seek(QTSS_StreamRef inRef, UInt64 inNewPosition);

/********************************************************************/
//  QTSS_Advise
//
//  Lets the stream know that the specified section of the stream will be read soon.
//
//  Arguments   inRef:          The stream to advise.
//              inPosition:     Offset from the start of the stream of the advise region.
//              inAdviseSize:   Size of the advise region.
//
//  Returns:    QTSS_NoErr
//              QTSS_RequestFailed
//              QTSS_BadArgument
QTSS_Error QTSS_Advise(QTSS_StreamRef inRef, UInt64 inPosition, UInt32 inAdviseSize);


/*****************************************/
//  SERVICES
//
//  Oftentimes modules have functionality that they want accessable from other
//  modules. An example of this might be a logging module that allows other
//  modules to write messages to the log.
//
//  Modules can use the following callbacks to register and invoke "services".
//  Adding & finding services works much like adding & finding attributes in
//  an object. A service has a name. In order to invoke a service, the calling
//  module must know the name of the service and resolve that name into an ID.
//
//  Each service has a parameter block format that is specific to that service.
//  Modules that are exporting services should carefully document the services they
//  export, and modules calling services should take care to fail gracefully
//  if the service isn't present or returns an error.

typedef QTSS_Error (*QTSS_ServiceFunctionPtr)(QTSS_ServiceFunctionArgsPtr);

/********************************************************************/
//  QTSS_AddService
//
//  This function registers a service with the specified name, and
//  associates it with the specified function pointer.
//  QTSS_AddService may only be called from the QTSS_Register role
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Service name too long, or NULL arguments.
QTSS_Error QTSS_AddService(char const *inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr);


/********************************************************************/
//  QTSS_IDForService
//
//  Much like QTSS_IDForAttr, this resolves a service name into its
//  corresponding QTSS_ServiceID. The QTSS_ServiceID can then be used to
//  invoke the service.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_IDForService(char const *inTag, QTSS_ServiceID *outID);

/********************************************************************/
//  QTSS_DoService
//
//  Invokes the service. Return value from this function comes from the service
//  function itself, unless the QTSS_IllegalService errorcode is returned,
//  which is returned when the QTSS_ServiceID is bad.
QTSS_Error QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs);

/********************************************************************/
//  BUILT-IN SERVICES
//
//  The server registers some built-in services when it starts up.
//  Here are macros for their names & descriptions of what they do

// Rereads the preferences, also causes the QTSS_RereadPrefs_Role to be invoked
#define QTSS_REREAD_PREFS_SERVICE   "RereadPreferences"



/*****************************************/
//  RTSP HEADER CALLBACKS
//
//  As a convience to modules that want to send RTSP responses, the server
//  has internal utilities for formatting a proper RTSP response. When a module
//  calls QTSS_SendRTSPHeaders, the server sends a proper RTSP status line, using
//  the request's current status code, and also sends the proper CSeq header,
//  session ID header, and connection header.
//
//  Any other headers can be appended by calling QTSS_AppendRTSPHeader. They will be
//  sent along with everything else when QTSS_SendRTSPHeaders is called.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_SendRTSPHeaders(QTSS_RTSPRequestObject inRef);
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_AppendRTSPHeader(QTSS_RTSPRequestObject inRef, QTSS_RTSPHeader inHeader, char const *inValue, UInt32 inValueLen);


/*****************************************/
//  QTSS_SendStandardRTSPResponse
//
//  This function is also provided as an optional convienence to modules who are sending
//  "typical" RTSP responses to clients. The function uses the QTSS_RTSPRequestObject and
//  the QTSS_Object as inputs, where the object may either be a QTSS_ClientSessionObject
//  or a QTSS_RTPStreamObject, depending on the method. The response is written to the
//  stream provided.
//
//  Below is a description of what is returned for each method this function supports:
//
//  DESCRIBE:
//
//   Writes status line, CSeq, SessionID, Connection headers as determined by the request.
//   Writes a Content-Base header with the Content-Base being the URL provided.
//   Writes a Content-Type header of "application/sdp"
//   QTSS_Object must be a QTSS_ClientSessionObject.
//
//  SETUP:
//
//   Writes status line, CSeq, SessionID, Connection headers as determined by the request.
//   Writes a Transport header with the client & server ports (if connection is over UDP).
//   QTSS_Object must be a QTSS_RTPStreamObject.
//
//  PLAY:
//
//   Writes status line, CSeq, SessionID, Connection headers as determined by the request.
//   QTSS_Object must be a QTSS_ClientSessionObject.
//
//   Specify whether you want the server to append the seq#, timestamp, & ssrc info to
//   the RTP-Info header via. the qtssPlayRespWriteTrackInfo flag.
//
//  PAUSE:
//
//   Writes status line, CSeq, SessionID, Connection headers as determined by the request.
//   QTSS_Object must be a QTSS_ClientSessionObject.
//
//  TEARDOWN:
//
//   Writes status line, CSeq, SessionID, Connection headers as determined by the request.
//   QTSS_Object must be a QTSS_ClientSessionObject.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_SendStandardRTSPResponse(QTSS_RTSPRequestObject inRTSPRequest, QTSS_Object inRTPInfo, UInt32 inFlags);


/*****************************************/
//  CLIENT SESSION CALLBACKS
//
//  QTSS API Modules have the option of generating and sending RTP packets. Only
//  one module currently can generate packets for a particular session. In order
//  to do this, call QTSS_AddRTPStream. This must be done in response to a RTSP
//  request, and typically is done in response to a SETUP request from the client.
//
//  After one or more streams have been added to the session, the module that "owns"
//  the packet sending for that session can call QTSS_Play to start the streams playing.
//  After calling QTSS_Play, the module will get invoked in the QTSS_SendPackets_Role.
//  Calling QTSS_Pause stops playing.
//
//  The "owning" module may call QTSS_Teardown at any time. Doing this closes the
//  session and will cause the QTSS_SessionClosing_Role to be invoked for this session. 
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_RequestFailed: QTSS_RTPStreamObject couldn't be created.
QTSS_Error QTSS_AddRTPStream(QTSS_ClientSessionObject inClientSession, QTSS_RTSPRequestObject inRTSPRequest, QTSS_RTPStreamObject *outStream, QTSS_AddStreamFlags inFlags);
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_RequestFailed: No streams added to this session.
QTSS_Error QTSS_Play(QTSS_ClientSessionObject inClientSession, QTSS_RTSPRequestObject inRTSPRequest, QTSS_PlayFlags inPlayFlags);
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_Pause(QTSS_ClientSessionObject inClientSession);
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_Teardown(QTSS_ClientSessionObject inClientSession);

//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_RefreshTimeOut(QTSS_ClientSessionObject inClientSession);

/*****************************************/
//  FILE SYSTEM CALLBACKS
//
//  All modules that interact with the local file system should use these APIs instead
//  of the direct operating system calls.
//
//  This is for two reasons: 1) to ensure portability of your module across different
//  platforms such as Win32 and different versions of the UNIX operating system.
//
//  2)  To ensure your module will work properly if there is a 3rd party file system
//      or database that contains media files.

/********************************************************************/
//  QTSS_OpenFileObject
//
//  Arguments   inPath: a NULL-terminated C-string containing a full path to the file to open.
//                      inPath must be in the local (operating system) file system path style.
//              inFlags: desired flags.
//              outFileObject:  If function returns QTSS_NoErr, on output this will be a QTSS_Object
//                              for the file.
//
//  Returns:    QTSS_NoErr
//              QTSS_FileNotFound
//              QTSS_RequestFailed
//              QTSS_BadArgument
QTSS_Error QTSS_OpenFileObject(char *inPath, QTSS_OpenFileFlags inFlags, QTSS_Object *outFileObject);

/********************************************************************/
//  QTSS_CloseFileObject
//
//  Closes the file object.
//
//  Arguments:  inFileObject: the file to close
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument
QTSS_Error QTSS_CloseFileObject(QTSS_Object inFileObject);


/*****************************************/
//  SOCKET CALLBACKS
//
//  It is not necessary for a module that internally uses network I/O to go through
//  the QTSS API for their networking APIs. However, it is highly recommended
//  to use nonblocking network I/O from a module. With nonblocking network I/O, it
//  is very important to be able to receive socket events.
//
//  To facilitate this, QTSS API provides the following two callbacks to link external
//  sockets into the QTSS API streams framework.
//
//  Once a module has created a QTSS stream out of its socket, it is possible to use the
//  QTSS_RequestEvent callback to receive events on the socket. 


/********************************************************************/
//  QTSS_CreateStreamFromSocket
//
//  Creates a socket stream.
//
//  Arguments:  inFileDesc: the socket
//
//  Returns:    QTSS_NoErr
QTSS_Error QTSS_CreateStreamFromSocket(int inFileDesc, QTSS_SocketStream *outStream);


/********************************************************************/
//  QTSS_DestroySocketStream
//
//  Creates a socket stream.
//
//  Arguments:  inFileDesc: the socket
//
//  Returns:    QTSS_NoErr
QTSS_Error QTSS_DestroySocketStream(QTSS_SocketStream inStream);


/*****************************************/
//  ASYNC I/O CALLBACKS
//
//  QTSS modules must be kind in how they use the CPU. The server doesn't
//  prevent a poorly implemented QTSS module from hogging the processing
//  capability of the server, at the expense of other modules and other clients.
//
//  It is therefore imperitive that a module use non-blocking, or async, I/O.
//  If a module were to block, say, waiting to read file data off disk, this stall
//  would affect the entire server.
//
//  This problem is resolved in QTSS API in a number of ways.
//
//  Firstly, all QTSS_StreamRefs provided to modules are non-blocking, or async.
//  Modules should be prepared to receive EWOULDBLOCK errors in response to
//  QTSS_Read, QTSS_Write, & QTSS_WriteV calls, with certain noted exceptions
//  in the case of responding to RTSP requests.
//
//  Modules that open their own file descriptors for network or file I/O can
//  create separate threads for handling I/O. In this case, these descriptors
//  can remain blocking, as long as they always block on the private module threads.
//
//  In most cases, however, creating a separate thread for I/O is not viable for the
//  kind of work the module would like to do. For instance, a module may wish
//  to respond to a RTSP DESCRIBE request, but can't immediately because constructing
//  the response would require I/O that would block.
//
//  The problem is once the module returns from the QTSS_RTSPProcess_Role, the
//  server will mistakenly consider the request handled, and move on. It won't
//  know that the module has more work to do before it finishes processing the DESCRIBE.
//
//  In this case, the module needs to tell the server to delay processing of the
//  DESCRIBE request until the file descriptor's blocking condition is lifted.
//  The module can do this by using the provided "event" callback routines.

/**
 * @return QTSS_NoErr
 * @return QTSS_BadArgument: Bad argument
 * @return QTSS_OutOfState: if this callback is made from a role that doesn't allow async I/O events
 * @return QTSS_RequestFailed: Not currently possible to request an event.
 */

QTSS_Error QTSS_RequestEvent(QTSS_StreamRef inStream, QTSS_EventType inEventMask);
QTSS_Error QTSS_SignalStream(QTSS_StreamRef inStream, QTSS_EventType inEventMask);

QTSS_Error QTSS_SetIdleTimer(SInt64 inIdleMsec);
QTSS_Error QTSS_SetIntervalRoleTimer(SInt64 inIdleMsec);

QTSS_Error QTSS_RequestGlobalLock();
bool QTSS_IsGlobalLocked();
QTSS_Error QTSS_GlobalUnLock();


/*
 * AUTHENTICATE and AUTHORIZE CALLBACKS
 * All modules that want Authentication outside of the
 * QTSS_RTSPAuthenticate_Role must use the QTSS_Authenticate callback
 * and must pass in the request object
 *     All modules that want Authorization outside of the
 *     QTSS_RTSPAuthorize_Role should use the QTSS_Authorize callback
 *     and must pass in the request object
 */

/**
 * QTSS_Authenticate
 *
 * @param inAuthUserName           the username that is to be authenticated
 * @param inAuthResourceLocalPath  the resource that is to be authorized access
 * @param inAuthMoviesDir          the movies directory (reqd. for finding the access file)
 * @param inAuthRequestAction      the action that is performed for the resource
 * @param inAuthScheme             the authentication scheme (the password retrieved will be based on it)
 * @param ioAuthRequestObject      the request object
 *                                 The object is filled with the attributes passed in
 *
 * @return QTSS_NoErr
 * @return QTSS_BadArgument  if any of the input arguments are null
 */
QTSS_Error QTSS_Authenticate(char const *inAuthUserName, char const *inAuthResourceLocalPath, char const *inAuthMoviesDir,
                             QTSS_ActionFlags inAuthRequestAction, QTSS_AuthScheme inAuthScheme, QTSS_RTSPRequestObject ioAuthRequestObject);

/**
 * QTSS_Authorize
 *
 * @param inAuthRequestObject  the request object
 * @param outAuthRealm         the authentication realm
 * @param outAuthUserAllowed   true if user is allowed, and false otherwise
 *
 * @returns  QTSS_NoErr, QTSS_BadArgument
 */
QTSS_Error QTSS_Authorize(QTSS_RTSPRequestObject inAuthRequestObject, char **outAuthRealm, bool *outAuthUserAllowed);

void QTSS_LockStdLib();
void QTSS_UnlockStdLib();

// Get HLS Sessions(json)
void *Easy_GetRTSPPushSessions();

#ifdef QTSS_OLDROUTINENAMES

// Legacy routines
// QTSS_AddAttribute has been replaced by QTSS_AddStaticAttribute
QTSS_Error QTSS_AddAttribute(QTSS_ObjectType inObjectType, char const* inAttributeName, void* inUnused);

#endif

#ifdef __cplusplus
}
#endif

#endif
