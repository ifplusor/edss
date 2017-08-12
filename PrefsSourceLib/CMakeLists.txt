set(HEADER_FILES
        )
set(SOURCE_FILES
        FilePrefsSource.cpp
        XMLParser.cpp
        XMLPrefsParser.cpp)

add_library(PrefsSource STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(PrefsSource
        PUBLIC include)
target_link_libraries(PrefsSource
        PUBLIC CommonUtilities)
