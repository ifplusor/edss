set(HEADER_FILES
        include/QTAccessFile.h
        include/QTSSMemoryDeleter.h
        include/QTSSModuleUtils.h
        include/QTSSRollingLog.h
        include/SDPSourceInfo.h
        include/SourceInfo.h)

set(SOURCE_FILES
        QTAccessFile.cpp
        QTSSModuleUtils.cpp
        QTSSRollingLog.cpp
        SDPSourceInfo.cpp
        SourceInfo.cpp)

add_library(APICommonCode STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(APICommonCode
        PUBLIC include)
target_link_libraries(APICommonCode
        PUBLIC APIStub
        PUBLIC HTTPUtilities
        PUBLIC RTPMetaInfo)
