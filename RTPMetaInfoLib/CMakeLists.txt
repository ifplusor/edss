set(HEADER_FILES
        )
set(SOURCE_FILES
        RTPMetaInfoPacket.cpp)

add_library(RTPMetaInfo STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(RTPMetaInfo
        PUBLIC include)
target_link_libraries(RTPMetaInfo
        PUBLIC SafeStd)
