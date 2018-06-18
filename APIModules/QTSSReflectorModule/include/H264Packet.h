//
// Created by james on 6/17/18.
//

#ifndef _EDSS2_H264PACKET_H_
#define _EDSS2_H264PACKET_H_

#include <CF/Types.h>


/**
 * H.264 Packet
 *
 * @see  https://blog.csdn.net/chen495810242/article/details/39207305
 *
 *
 * NAL Unit Header:
 *
 *    +---------------+
 *    |0|1|2|3|4|5|6|7|
 *    +-+-+-+-+-+-+-+-+
 *    |F|NRI|  Type   |
 *    +---------------+
 *
 * @see  rfc6184(section 1.3)
 *
 */
struct NALUHeader {
#if BIGENDIAN
  unsigned char f:1;
  unsigned char nri:2;
  unsigned char type:5;
#else
  unsigned char type:5;
  unsigned char nri:2;
  unsigned char f:1;
#endif
};

/**
 * FU-A:
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    | FU indicator  |   FU header   |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 *    |                                                               |
 *    |                         FU payload                            |
 *    |                                                               |
 *    |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                               :...OPTIONAL RTP padding        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * @see  rfc6184(5.8)
 *
 */


/**
 * FU indicator:
 *
 *    Same as NAL Unit Header, but the Type is 28.
 *
 */
typedef NALUHeader FUIndicator;

/**
 * FU header:
 *
 *    +---------------+
 *    |0|1|2|3|4|5|6|7|
 *    +-+-+-+-+-+-+-+-+
 *    |S|E|R|  Type   |
 *    +---------------+
 *
 * @see  rfc6184(5.8)
 *
 */
struct FUHeader {
#if BIGENDIAN
#else
  unsigned char type : 5; //little 5 bit
  unsigned char r : 1;
  unsigned char e : 1;
  unsigned char s : 1; //high bit
#endif
};

#endif // _EDSS2_H264PACKET_H_
