set(HEADER_FILES
        )
set(SOURCE_FILES
        ClientSession.cpp)

add_library(RTSPClient STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(RTSPClient
        PUBLIC include)

