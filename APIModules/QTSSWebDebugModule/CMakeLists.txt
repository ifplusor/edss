set(HEADER_FILES
        include/QTSSWebDebugModule.h)

set(SOURCE_FILES
        QTSSWebDebugModule.cpp)

add_library(QTSSWebDebugModule STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSWebDebugModule
        PUBLIC include)

