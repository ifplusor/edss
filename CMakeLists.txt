cmake_minimum_required(VERSION 2.8)
project(edss2)

set(CMAKE_CXX_STANDARD 11)

if (NOT (DEFINED CxxFramework_DIR))
    set(CxxFramework_DIR "${PROJECT_SOURCE_DIR}/../CxxFramework")
endif()

if ((NOT (DEFINED DetectTarget)) OR DetectTarget)
    message("edss2: Detecting target...")
    LIST(APPEND CMAKE_MODULE_PATH ${CxxFramework_DIR}/cmake)
    include(DetectTarget)
    target_platform(CONF_PLATFORM)
    target_architecture(CONF_ARCH)
    set(DetectTarget FALSE)
endif ()

add_subdirectory(${CxxFramework_DIR} CxxFramework)
link_libraries(CxxFramework)

add_subdirectory(StreamingBase)
add_subdirectory(PrefsSourceLib)

add_subdirectory(APIStubLib)
add_subdirectory(APICommonCode)
add_subdirectory(QTFileLib)
add_subdirectory(APIModules)

add_subdirectory(Server.tproj)
