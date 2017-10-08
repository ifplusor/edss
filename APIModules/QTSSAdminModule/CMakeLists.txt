set(HEADER_FILES
        include/QTSSAdminModule.h
        AdminElementNode.h
        AdminQuery.h
        frozen.h
        mongoose.h)

set(SOURCE_FILES
        AdminElementNode.cpp
        AdminQuery.cpp
        QTSSAdminModule.cpp
        frozen.c
        mongoose.c)

add_library(QTSSAdminModule STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTSSAdminModule
        PUBLIC include)

