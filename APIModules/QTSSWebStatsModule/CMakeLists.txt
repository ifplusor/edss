set(HEADER_FILES
        )
set(SOURCE_FILES
        QTSSWebStatsModule.cpp)

add_library(QTSSWebStatsModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSWebStatsModule
        PUBLIC include)

