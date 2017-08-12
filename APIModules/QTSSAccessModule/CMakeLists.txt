set(HEADER_FILES
        )
set(SOURCE_FILES
        AccessChecker.cpp
        QTSSAccessModule.cpp)

add_library(QTSSAccessModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSAccessModule
        PUBLIC include)

