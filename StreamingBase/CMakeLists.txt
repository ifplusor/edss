set(HEADER_FILES
        include/SDPUtils.h
        include/RTPMetaInfoPacket.h)

set(SOURCE_FILES
        SDPUtils.cpp
        RTPMetaInfoPacket.cpp)

add_library(StreamingBase STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(StreamingBase
        PUBLIC include)
