set(HEADER_FILES
        include/SafeStdLib.h)

set(SOURCE_FILES
        InternalStdLib.cpp)

add_library(SafeStd STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(SafeStd
        PUBLIC include)
target_link_libraries(SafeStd
        PUBLIC CommonUtilities)
