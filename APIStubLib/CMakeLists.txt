set(HEADER_FILES
        include/QTSS.h
        include/QTSS_Private.h
        include/QTSSRTSPProtocol.h)

set(SOURCE_FILES
        QTSS_Private.cpp)

add_library(APIStub STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(APIStub
        PUBLIC include)
