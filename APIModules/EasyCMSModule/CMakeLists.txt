set(HEADER_FILES
        )
set(SOURCE_FILES
        EasyCMSModule.cpp
        EasyCMSSession.cpp)

add_library(EasyCMSModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(EasyCMSModule
        PUBLIC include)

