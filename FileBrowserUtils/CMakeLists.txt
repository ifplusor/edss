set(HEADER_FILES
        )
set(SOURCE_FILES
        SearchFileDir.cpp)

add_library(FileBrowserUtils STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(FileBrowserUtils
        PUBLIC include)
