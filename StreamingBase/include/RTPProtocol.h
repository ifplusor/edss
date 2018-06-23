//
// Created by James on 2018/6/22.
//

#ifndef _EDSS2_RTP_PROTOCOL_H_
#define _EDSS2_RTP_PROTOCOL_H_

#include <CF/Types.h>


/**
 * 关于字节序和位序:
 *
 * 一般讲大端序和小端序时，指的都是字节序，即：
 *   - 大端序：高位低址，低位高址
 *   - 小端序：低位低址，高位高址
 *
 * 常见的字节序应用场景是网络数据传输，此时区分网络字节序和主机字节序。
 *
 * 而位序则与位域的布局分配有关，一般来说位序和字节序保持一致。
 * 需要记住的一个基本原则是：连续分配。即：跨字节的位域在逻辑上也是连续的。
 *
 * 备注：
 *   需要注意的是：htonl、ntohl等转换函数“只转换字节序，不转换位序”！
 *
 *   尽管在以太网(Ethernet)中，bit的传输遵循：从最低有效比特位到最高有效比特位的发送顺序。
 *   但这一规则是发生在网卡中的，对上层的CPU、OS等都不可见，因此在传输过程中不会出现字节值变化的问题。
 *
 */

/**
 * RTP Fixed Header Fields:
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                           timestamp                           |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |           synchronization source (SSRC) identifier            |
 *    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *    |            contributing source (CSRC) identifiers             |
 *    |                             ....                              |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * @see  rfc1889(section 5.1)
 *
 */
typedef struct RTPFixedHeader {
#if BIGENDIAN
  unsigned char v:2;                /* protocol version */
  unsigned char p:1;                /* padding flag */
  unsigned char x:1;                /* header extension flag */
  unsigned char cc:4;               /* CSRC count */
  unsigned char m:1;                /* marker bit */
  unsigned char pt:7;               /* payload type */
#else
  unsigned char cc:4;               /* CSRC count */
  unsigned char x:1;                /* header extension flag */
  unsigned char p:1;                /* padding flag */
  unsigned char v:2;                /* protocol version */
  unsigned char pt:7;               /* payload type */
  unsigned char m:1;                /* marker bit */
#endif
  UInt16 seq;				       /* sequence number */
  UInt32 ts;                       /* timestamp */
  UInt32 ssrc;                     /* synchronization source */
  UInt32 csrc[0];                  /* optional CSRC list */
} RTPFixedHeader;

/**
 * RTP Header Extension:
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |      defined by profile       |           length              |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        header extension                       |
 *    |                             ....                              |
 *
 * @see  rfc1889(section 5.3.1)
 *
 *
 * @note  RTP data packets contain no length field or other delineation,
 *   therefore RTP relies on the underlying protocol(s) to provide a
 *   length indication. The maximum length of RTP packets is limited only
 *   by the underlying protocols.
 *
 * @see  rfc1889(section 10)
 *
 */
typedef struct RTPExtHeader {
  UInt16 profile;
  UInt16 length;
  char ext[0];
} RTPExtHeader;

#endif // _EDSS2_RTP_PROTOCOL_H_
