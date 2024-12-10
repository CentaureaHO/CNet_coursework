#ifndef __NET_RUDP_RUDP_DEFS_H__
#define __NET_RUDP_RUDP_DEFS_H__

#include <stdint.h>
#include <string>

#define PACKET_SIZE 15000
#define BODY_SIZE (PACKET_SIZE - sizeof(RUDP_H))

#define RUDP_STATU_LIST  \
    X(CLOSED, b, 0)      \
    X(LISTEN, s, 1)      \
    X(SYN_SENT, c, 2)    \
    X(SYN_RCVD, s, 3)    \
    X(ESTABLISHED, b, 4) \
    X(FIN_WAIT, b, 5)    \
    X(FIN_RCVD, b, 6)    \
    X(CLOSE_WAIT, b, 7)

enum class RUDP_STATUS
{
#define X(name, role, idx) name = idx,
    RUDP_STATU_LIST
#undef X
};

#pragma pack(1)

struct RUDP_H
{
    uint32_t connect_id;
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t data_len;
    uint16_t flags, checksum;

    /*
     *  flags:
     *  flags[0]: SYN   0b0000_0000_0000_0001   0x0001
     *  flags[1]: ACK   0b0000_0000_0000_0010   0x0002
     *  flags[2]: FIN   0b0000_0000_0000_0100   0x0004
     *  flags[3]: RST   0b0000_0000_0000_1000   0x0008
     */

    RUDP_H();
};

struct RUDP_P
{
    RUDP_H header;
    char   body[BODY_SIZE];
};

#pragma pack()

uint16_t lenInByte(const RUDP_P& packet);

uint16_t genCheckSum(RUDP_P& packet);
bool     checkCheckSum(RUDP_P& packet);

#define SET_SYN(rudp) (rudp.header.flags |= 0x0001)
#define SET_ACK(rudp) (rudp.header.flags |= 0x0002)
#define SET_FIN(rudp) (rudp.header.flags |= 0x0004)
#define SET_RST(rudp) (rudp.header.flags |= 0x0008)

#define CHK_SYN(rudp) (rudp.header.flags & 0x0001)
#define CHK_ACK(rudp) (rudp.header.flags & 0x0002)
#define CHK_FIN(rudp) (rudp.header.flags & 0x0004)
#define CHK_RST(rudp) (rudp.header.flags & 0x0008)

#define CLR_FLAGS(rudp) (rudp.header.flags = 0x0000)
#define CLR_PACKET(rudp)          \
    {                             \
        CLR_FLAGS(rudp);          \
        rudp.header.seq_num  = 0; \
        rudp.header.ack_num  = 0; \
        rudp.header.data_len = 0; \
    }

#define SET_SYN_H(rudp) (rudp.flags |= 0x0001)
#define SET_ACK_H(rudp) (rudp.flags |= 0x0002)
#define SET_FIN_H(rudp) (rudp.flags |= 0x0004)
#define SET_RST_H(rudp) (rudp.flags |= 0x0008)

#define CHK_SYN_H(rudp) (rudp.flags & 0x0001)
#define CHK_ACK_H(rudp) (rudp.flags & 0x0002)
#define CHK_FIN_H(rudp) (rudp.flags & 0x0004)
#define CHK_RST_H(rudp) (rudp.flags & 0x0008)

#define CLR_FLAGS_H(rudp) (rudp.flags = 0x0000)

std::string   statuStr(RUDP_STATUS statu);
std::string   flagsToStr(const RUDP_P& p);
std::ostream& operator<<(std::ostream& os, const RUDP_H& header);

#endif