#include <net/rudp/rudp_defs.h>
#include <iostream>
using namespace std;

RUDP_H::RUDP_H() : seq_num(0), ack_num(0), data_len(0), flags(0), checksum(0) {}

uint16_t lenInByte(const RUDP_P& packet) { return sizeof(RUDP_H) + packet.header.data_len; }

uint16_t genCheckSum(RUDP_P& packet)
{
    packet.header.checksum = 0;
    uint32_t  sum          = 0;
    uint16_t* data         = reinterpret_cast<uint16_t*>(&packet);

    for (size_t i = 0; i < lenInByte(packet) / 2; ++i)
    {
        sum += data[i];
        if (sum > 0xFFFF) sum -= 0xFFFF;
    }

    if (lenInByte(packet) % 2 != 0)
    {
        sum += static_cast<uint8_t*>(reinterpret_cast<void*>(&packet))[lenInByte(packet) - 1];
        if (sum > 0xFFFF) sum -= 0xFFFF;
    }

    packet.header.checksum = ~sum;
    return ~sum;
}

// 校验数据包的校验和
bool checkCheckSum(RUDP_P& packet)
{
    uint16_t received_checksum = packet.header.checksum;
    uint16_t calculated_checksum = genCheckSum(packet);
    packet.header.checksum = received_checksum;

    return received_checksum == calculated_checksum;
}