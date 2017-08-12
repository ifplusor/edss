set(HEADER_FILES
        )
set(SOURCE_FILES
        QTSSFlowControlModule.cpp)

add_library(QTSSFlowControlModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSFlowControlModule
        PUBLIC include)

