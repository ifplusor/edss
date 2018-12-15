add_subdirectory(RTSPUtilitiesLib)
add_subdirectory(RTPMetaInfoLib)
add_subdirectory(RTCPUtilitiesLib)


set(HEADER_FILES
        include/SDPUtils.h
        include/FileCache.h
        include/UserAgentParser.h
        include/KeyFrameCache.h
        include/RTPProtocol.h
        include/H264Packet.h)

set(SOURCE_FILES
        SDPUtils.cpp
        FileCache.cpp
        UserAgentParser.cpp
        KeyFrameCache.cpp
        H264Packet.cpp)

#if ((${CONF_PLATFORM} STREQUAL "Win32") OR (${CONF_PLATFORM} STREQUAL "MinGW"))
#    set(HEADER_FILES ${HEADER_FILES} include/CreateDump.h)
#    set(SOURCE_FILES ${SOURCE_FILES} CreateDump.cpp)
#endif ()

add_library(StreamingBase STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(StreamingBase
        PUBLIC include)
target_link_libraries(StreamingBase
        PUBLIC RTSPUtilities
        PUBLIC RTPMetaInfo
        PUBLIC RTCPUtilities)
