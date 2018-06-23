set(HEADER_FILES
        include/SequenceNumberMap.h
        include/ReflectorOutput.h
        include/ReflectorStream.h
        include/ReflectorSession.h
        include/QTSSReflectorModule.h
#        RCFSourceInfo.h
        RTPSessionOutput.h)

set(SOURCE_FILES
        QTSSReflectorModule.cpp
#        RCFSourceInfo.cpp
        RTPSessionOutput.cpp
        ReflectorSession.cpp
        ReflectorStream.cpp
        SequenceNumberMap.cpp)

add_library(QTSSReflectorModule STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSReflectorModule
        PUBLIC include)
target_link_libraries(QTSSReflectorModule
        PUBLIC StreamingBase)
