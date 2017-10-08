set(HEADER_FILES
        include/RTCPPacket.h
        include/RTCPAPPPacket.h
        include/RTCPAPPNADUPacket.h
        include/RTCPAPPQTSSPacket.h
        include/RTCPAckPacket.h
        include/RTCPSRPacket.h)

set(SOURCE_FILES
        RTCPPacket.cpp
        RTCPAPPPacket.cpp
        RTCPAPPNADUPacket.cpp
        RTCPAPPQTSSPacket.cpp
        RTCPAckPacket.cpp
        RTCPSRPacket.cpp)

add_library(RTCPUtilities STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(RTCPUtilities
        PUBLIC include)
