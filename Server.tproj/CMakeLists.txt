set(HEADER_FILES
        include/defaultPaths.h

        RTSPRequestStream.h
        RTSPResponseStream.h
        RTSPSessionInterface.h
        RTSPRequestInterface.h
        RTSPRequest.h
        RTSPSession.h
        RTPBandwidthTracker.h
        RTPOverbufferWindow.h
        RTPPacketResender.h
        RTPStream.h
        RTPSessionInterface.h
        RTPSession.h
        RTCPTask.h

        QTSSDataConverter.h
        QTSSUserProfile.h
        QTSSFile.h
        QTSSSocket.h
        QTSSCallbacks.h
        include/QTSSStream.h
        include/QTSSDictionary.h
        include/QTSSPrefs.h
        include/QTSServerPrefs.h
        include/QTSSMessages.h
        include/QTSSModule.h
        include/QTSServerInterface.h
        include/QTSServer.h
        GenerateXMLPrefs.h
        RunServer.h)
#        EDSS.h)

set(SOURCE_FILES
        RTSPRequestStream.cpp
        RTSPResponseStream.cpp
        RTSPSessionInterface.cpp
        RTSPRequestInterface.cpp
        RTSPRequest.cpp
        RTSPSession.cpp
        RTPBandwidthTracker.cpp
        RTPOverbufferWindow.cpp
        RTPPacketResender.cpp
        RTPStream.cpp
        RTPSessionInterface.cpp
        RTPSession.cpp
        RTCPTask.cpp

        QTSSDataConverter.cpp
        QTSSUserProfile.cpp
        QTSSDictionary.cpp
        QTSSPrefs.cpp
        QTSServerPrefs.cpp
        QTSSMessages.cpp
        QTSSModule.cpp
        QTSSFile.cpp
        QTSServerInterface.cpp
        QTSSSocket.cpp
        QTSSCallbacks.cpp
        QTSServer.cpp
        GenerateXMLPrefs.cpp
        RunServer.cpp
        EDSS.cpp)

if ((${CONF_PLATFORM} STREQUAL "Win32")
        OR (${CONF_PLATFORM} STREQUAL "MinGW"))
    set(SOURCE_FILES ${SOURCE_FILES} win32main.cpp)
else()
    set(SOURCE_FILES ${SOURCE_FILES} main.cpp)
endif()


add_executable(edss2
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(edss2
        PUBLIC include)
target_link_libraries(edss2
        PRIVATE PrefsSource
        PRIVATE RTSPUtilities
        PRIVATE RTCPUtilities
        PRIVATE APICommonCode
        PRIVATE QTFile
        PRIVATE QTSSErrorLogModule
        PRIVATE QTSSAccessLogModule
        PRIVATE QTSSAccessModule
        PRIVATE QTSSFlowControlModule
        PRIVATE QTSSPOSIXFileSysModule
        PRIVATE QTSSReflectorModule)

IF (__PTHREADS__)
    target_link_libraries(edss2
            PRIVATE pthread)
ENDIF (__PTHREADS__)

IF (${CONF_PLATFORM} STREQUAL "Linux")
    target_link_libraries(edss2
            PRIVATE stdc++
            PRIVATE m
            PRIVATE crypt)
ENDIF ()

