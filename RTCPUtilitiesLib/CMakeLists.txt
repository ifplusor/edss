set(HEADER_FILES
        )
set(SOURCE_FILES
        RTCPAPPNADUPacket.cpp
        RTCPAPPPacket.cpp
        RTCPAPPQTSSPacket.cpp
        RTCPAckPacket.cpp
        RTCPPacket.cpp
        RTCPSRPacket.cpp)

add_library(RTCPUtilities STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(RTCPUtilities
        PUBLIC include)
target_link_libraries(RTCPUtilities
        PUBLIC CommonUtilities)
