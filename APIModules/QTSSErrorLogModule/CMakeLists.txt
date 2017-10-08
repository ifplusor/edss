set(HEADER_FILES
        include/QTSSErrorLogModule.h)

set(SOURCE_FILES
        QTSSErrorLogModule.cpp)

add_library(QTSSErrorLogModule STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSErrorLogModule
        PUBLIC include)

