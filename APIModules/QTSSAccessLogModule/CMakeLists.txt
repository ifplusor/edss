set(HEADER_FILES
        )
set(SOURCE_FILES
        QTSSAccessLogModule.cpp)

add_library(QTSSAccessLogModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSAccessLogModule
        PUBLIC include)

