set(HEADER_FILES
        )
set(SOURCE_FILES
        QTSSReflectorModule.cpp
        RCFSourceInfo.cpp
        RTPSessionOutput.cpp
        ReflectorSession.cpp
        ReflectorStream.cpp
        SequenceNumberMap.cpp)

add_library(QTSSReflectorModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSReflectorModule
        PUBLIC include)
target_link_libraries(QTSSReflectorModule
        PUBLIC RTCPUtilities
        PUBLIC EasyProtocol)
