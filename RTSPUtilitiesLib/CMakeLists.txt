set(HEADER_FILES
        )
set(SOURCE_FILES
        RTSPProtocol.cpp)

add_library(RTSPUtilities STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(RTSPUtilities
        PUBLIC include)
target_link_libraries(RTSPUtilities
        PUBLIC CommonUtilities)
