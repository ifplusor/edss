set(HEADER_FILES
        )
set(SOURCE_FILES
        EasyRedisModule.cpp)

add_library(EasyRedisModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(EasyRedisModule
        PUBLIC include)

