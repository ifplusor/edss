cmake_minimum_required(VERSION 2.8)
project(edss2)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(${PROJECT_SOURCE_DIR}/../CxxFramework CxxFramework)
link_libraries(CxxFramework)

add_subdirectory(StreamingBase)

add_subdirectory(APIStubLib)
add_subdirectory(APICommonCode)

set(SOURCE_FILES main.cpp)
add_executable(edss2 ${SOURCE_FILES})