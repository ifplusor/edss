set(HEADER_FILES
        )
set(SOURCE_FILES
        src/lib_json/json_reader.cpp
        src/lib_json/json_value.cpp
        src/lib_json/json_writer.cpp)

add_library(json STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(json
        PUBLIC include)
