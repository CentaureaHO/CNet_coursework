#ifndef __NET_PACKET_H__
#define __NET_PACKET_H__

#include <cstdint>

#define BIT_MASK(u32, mask) (u32 & mask)

#define DATA_LENTH(dlh, dlm, dll) (((uint32_t)dlh << 16) | ((uint32_t)dlm << 8) | ((uint32_t)dll))
#define DATA_LENTH_H(dl) (uint8_t)(dl >> 16)
#define DATA_LENTH_M(dl) (uint8_t)(dl >> 8)
#define DATA_LENTH_L(dl) (uint8_t)(dl)

#define U32_H16(u32) (uint16_t)(u32 >> 16)
#define U32_L16(u32) (uint16_t)(u32)
#define MASK_U32_L16 0x0000FFFF

#define CID(cidh, cidl) (((uint64_t)cidh << 32) | (uint64_t)cidl)
#define CID_H(cid) (uint32_t)(cid >> 32)
#define CID_L(cid) (uint32_t)(cid)

#define SYN 0b00000001
#define ACK 0b00000010
#define FIN 0b00000100

#define HEADER_LEN (sizeof(RUHeader) / sizeof(char))

struct RUHeader
{
    /*
      Packet header structure:
      |                            32bits                             |
      |---------------------------------------------------------------|
      |  Common Flags(8bits)  |         Data Length(24bits)           |
      |---------------------------------------------------------------|
      |                         Seq ID(32bits)                        |
      |---------------------------------------------------------------|
      |                         Ack ID(32bits)                        |
      |---------------------------------------------------------------|
      |                   Connection ID H(32bits)                     |
      |---------------------------------------------------------------|
      |                   Connection ID L(32bits)                     |
      |---------------------------------------------------------------|
      |        Sum Check(16bits)      |       Reserved(16bits)        |
      |---------------------------------------------------------------|
     */
    /*
        Common Flags:
        |     7     |     6     |     5     |     4     |     3     |     2     |     1     |     0     |
        |-----------|-----------|-----------|-----------|-----------|-----------|-----------|-----------|
        |  Reserved |  Reserved |  Reserved |  Reserved |  Reserved |     FIN   |     ACK   |     SYN   |

     */

    uint8_t  flags;
    uint8_t  dlh, dlm, dll;
    uint32_t seq_id, ack_id;
    uint64_t cid;
    uint16_t sum_check;
    uint16_t reserved;

    RUHeader() : flags(0), dlh(0), dlm(0), dll(0), seq_id(0), ack_id(0), cid(0), sum_check(0), reserved(0) {}
};

struct RUPacket
{
    RUHeader header;
    char*    data;

    RUPacket() : header(), data(nullptr) {}
};

uint16_t set_sum_check(RUPacket& packet);
bool     check_sum_check(const RUPacket& packet);

#endif  // __NET_PACKET_H__