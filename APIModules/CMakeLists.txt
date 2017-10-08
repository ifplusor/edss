include_directories(${PROJECT_SOURCE_DIR}/Server.tproj/include)
link_libraries(APICommonCode)
link_libraries(PrefsSource)

set_property(GLOBAL
        PROPERTY PREFIX "")

add_subdirectory(QTSSAccessLogModule)
add_subdirectory(QTSSAccessModule)
#add_subdirectory(QTSSAdminModule)
add_subdirectory(QTSSErrorLogModule)
add_subdirectory(QTSSFlowControlModule)
add_subdirectory(QTSSPOSIXFileSysModule)
add_subdirectory(QTSSReflectorModule)
add_subdirectory(QTSSWebDebugModule)

#add_subdirectory(EasyCMSModule)
#add_subdirectory(EasyRedisModule)
