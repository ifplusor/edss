set(HEADER_FILES
        include/XMLParser.h
        include/XMLPrefsParser.h
        include/PrefsSource.h
        include/FilePrefsSource.h)

set(SOURCE_FILES
        XMLParser.cpp
        XMLPrefsParser.cpp
        FilePrefsSource.cpp)

add_library(PrefsSource STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(PrefsSource
        PUBLIC include)
