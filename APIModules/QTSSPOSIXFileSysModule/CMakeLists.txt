set(HEADER_FILES
        include/QTSSPosixFileSysModule.h)

set(SOURCE_FILES
        QTSSPosixFileSysModule.cpp)

add_library(QTSSPOSIXFileSysModule STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSPOSIXFileSysModule
        PUBLIC include)

