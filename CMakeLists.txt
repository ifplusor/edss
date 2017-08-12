cmake_minimum_required(VERSION 2.8)
project(edss)

# detect os
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CND_PLATFORM OSX)
    set(CND_DLIB_EXT so)
    add_definitions(-D__MacOSX__)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CND_PLATFORM Linux)
    set(CND_DLIB_EXT so)
    add_definitions(-D__linux__)
    #    add_compile_options(-O2)
    add_compile_options(-g)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CND_PLATFORM Win32)
    set(CND_DLIB_EXT dll)
    add_definitions(-D__Win32__)
else ()
    message("no build confg for ${CMAKE_SYSTEM_NAME}")
endif ()

# detect architecture
include(TargetArch.cmake)
target_architecture(CDN_ARCH)

message("build for ${CND_PLATFORM}-${CDN_ARCH}")

# set language standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# set output directory
set(OUTPUT_PATH ${PROJECT_BINARY_DIR}/${CND_PLATFORM}-${CDN_ARCH})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_PATH})


add_subdirectory(ThirdParty/jsoncpp)
add_subdirectory(EasyProtocolLib)

add_subdirectory(CommonUtilitiesLib)
add_subdirectory(SafeStdLib)

add_subdirectory(APIStubLib)
add_subdirectory(APICommonCode)

add_subdirectory(HTTPUtilitiesLib)
add_subdirectory(RTSPUtilitiesLib)
add_subdirectory(RTCPUtilitiesLib)
add_subdirectory(RTPMetaInfoLib)

add_subdirectory(QTFileLib)
add_subdirectory(APIModules)

add_subdirectory(PrefsSourceLib)

add_subdirectory(Server.tproj)


