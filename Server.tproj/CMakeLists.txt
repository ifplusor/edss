set(HEADER_FILES
        )
set(SOURCE_FILES
        GenerateXMLPrefs.cpp
        HTTPSession.cpp
        HTTPSessionInterface.cpp
        QTSSCallbacks.cpp
        QTSSDataConverter.cpp
        QTSSDictionary.cpp
        QTSSErrorLogModule.cpp
        QTSSExpirationDate.cpp
        QTSSFile.cpp
        QTSSMessages.cpp
        QTSSModule.cpp
        QTSSPrefs.cpp
        QTSSSocket.cpp
        QTSSUserProfile.cpp
        QTSServer.cpp
        QTSServerInterface.cpp
        QTSServerPrefs.cpp
        RTCPTask.cpp
        RTPBandwidthTracker.cpp
        RTPOverbufferWindow.cpp
        RTPPacketResender.cpp
        RTPSession.cpp
        RTPSessionInterface.cpp
        RTPStream.cpp
        RTSPRequest.cpp
        RTSPRequestInterface.cpp
        RTSPRequestStream.cpp
        RTSPResponseStream.cpp
        RTSPSession.cpp
        RTSPSessionInterface.cpp
        RunServer.cpp
        main.cpp)

add_executable(edss ${SOURCE_FILES})
target_include_directories(edss
        PRIVATE include
        PRIVATE ${PROJECT_SOURCE_DIR}/Include/)
target_link_libraries(edss
        PRIVATE CommonUtilities
        PRIVATE APICommonCode
        PRIVATE QTFile
        PRIVATE RTSPUtilities
        PRIVATE PrefsSource
        PRIVATE RTCPUtilities
        PRIVATE EasyProtocol
        PRIVATE QTSSAccessLogModule
        PRIVATE QTSSAccessModule
        PRIVATE QTSSFlowControlModule
        PRIVATE QTSSPOSIXFileSysModule
        PRIVATE QTSSReflectorModule
        PRIVATE pthread
        PRIVATE dl
        PRIVATE stdc++
        PRIVATE m
        PRIVATE crypt)



