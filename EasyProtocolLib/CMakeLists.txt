set(HEADER_FILES
        include/EasyProtocol.h
        include/EasyProtocolBase.h
        include/EasyProtocolDef.h
        include/EasyUtil.h)

set(SOURCE_FILES
        EasyProtocol.cpp
        EasyProtocolBase.cpp
        EasyUtil.cpp)

add_library(EasyProtocol STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(EasyProtocol
        PUBLIC include
        PUBLIC ${PROJECT_SOURCE_DIR}/ThirdParty)
target_link_libraries(EasyProtocol
        PUBLIC json)