set(HEADER_FILES
        include/RTPMetaInfoPacket.h)
set(SOURCE_FILES
        RTPMetaInfoPacket.cpp)

add_library(RTPMetaInfo STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(RTPMetaInfo
        PUBLIC include)
