add_subdirectory(RTSPUtilitiesLib)
add_subdirectory(RTPMetaInfoLib)
add_subdirectory(RTCPUtilitiesLib)


set(HEADER_FILES
        include/SDPUtils.h
        include/SDPCache.h
        include/UserAgentParser.h
        include/KeyFrameCache.h)

set(SOURCE_FILES
        SDPUtils.cpp
        SDPCache.cpp
        UserAgentParser.cpp
        KeyFrameCache.cpp)

add_library(StreamingBase STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(StreamingBase
        PUBLIC include)
target_link_libraries(StreamingBase
        PUBLIC RTSPUtilities
        PUBLIC RTPMetaInfo
        PUBLIC RTCPUtilities)
