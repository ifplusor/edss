set(HEADER_FILES
        include/QTSSAccessModule.h
        AccessChecker.h)

set(SOURCE_FILES
        AccessChecker.cpp
        QTSSAccessModule.cpp)

add_library(QTSSAccessModule STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSAccessModule
        PUBLIC include)

