
add_definitions(-D_REENTRANT -D__USE_POSIX)
add_compile_options(-include ../Include/PlatformHeader.h)


set(HEADER_FILES
        )
set(SOURCE_FILES
        QTAccessFile.cpp
        QTSSModuleUtils.cpp
        QTSSRollingLog.cpp
        SDPSourceInfo.cpp
        SourceInfo.cpp)

add_library(APICommonCode STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(APICommonCode
        PUBLIC include)

