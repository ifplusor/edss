//
// Created by james on 6/19/18.
//

#include <H264Packet.h>


char const* nal_unit_type_description_h264[32] = {
    "Unspecified", //0
    "Coded slice of a non-IDR picture", //1
    "Coded slice data partition A", //2
    "Coded slice data partition B", //3
    "Coded slice data partition C", //4
    "Coded slice of an IDR picture", //5
    "Supplemental enhancement information (SEI)", //6
    "Sequence parameter set", //7
    "Picture parameter set", //8
    "Access unit delimiter", //9
    "End of sequence", //10
    "End of stream", //11
    "Filler data", //12
    "Sequence parameter set extension", //13
    "Prefix NAL unit", //14
    "Subset sequence parameter set", //15
    "Reserved", //16
    "Reserved", //17
    "Reserved", //18
    "Coded slice of an auxiliary coded picture without partitioning", //19
    "Coded slice extension", //20
    "Reserved", //21
    "Reserved", //22
    "Reserved", //23
    "Unspecified", //24
    "Unspecified", //25
    "Unspecified", //26
    "Unspecified", //27
    "Unspecified", //28
    "Unspecified", //29
    "Unspecified", //30
    "Unspecified" //31
};

