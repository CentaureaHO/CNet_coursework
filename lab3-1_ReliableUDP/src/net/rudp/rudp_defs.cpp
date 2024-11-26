#include <net/rudp/rudp_defs.h>
#include <iostream>
using namespace std;

RUDP_H::RUDP_H() : seq_num(0), ack_num(0), data_len(0), flags(0), checksum(0) {}

uint16_t lenInByte(const RUDP_P& packet) { return sizeof(RUDP_H) + packet.header.data_len; }

uint16_t genCheckSum(RUDP_P& packet)
{
    const uint16_t* cur_pos = reinterpret_cast<uint16_t*>(&packet);
    const uint16_t* sum_pos = reinterpret_cast<uint16_t*>(&packet.header.checksum);

    uint32_t res = 0;
    while (cur_pos < sum_pos)
    {
        res += *cur_pos;
        while (res >> 16) res = (res & 0xFFFF) + (res >> 16);
        ++cur_pos;
    }

    if (packet.header.data_len != 0)
    {
        if (packet.header.data_len % 2) packet.body[packet.header.data_len] = 0x0;

        cur_pos                      = (uint16_t*)(&packet.body);
        const uint16_t* data_end_pos = (uint16_t*)(&packet.body[packet.header.data_len + 1]);

        while (cur_pos < data_end_pos)
        {
            res += *cur_pos;
            while (res >> 16) res = (res & 0xFFFF) + (res >> 16);
            ++cur_pos;
        }
    }

    packet.header.checksum = (uint16_t)(~res);

    return packet.header.checksum;
}

bool checkCheckSum(RUDP_P& packet)
{
    /*
    const uint16_t* cur_pos = reinterpret_cast<uint16_t*>(&packet);
    const uint16_t* sum_pos = reinterpret_cast<uint16_t*>(&packet.header.checksum);

    uint32_t res = 0;
    while (cur_pos < sum_pos)
    {
        res += *cur_pos;
        while (res >> 16) res = (res & 0xFFFF) + (res >> 16);
        ++cur_pos;
    }

    if (packet.header.data_len != 0)
    {
        if (packet.header.data_len % 2) packet.body[packet.header.data_len] = 0x0;

        cur_pos                      = reinterpret_cast<uint16_t*>(&packet.body);
        const uint16_t* data_end_pos = reinterpret_cast<uint16_t*>(&packet.body[packet.header.data_len + 1]);

        while (cur_pos < data_end_pos)
        {
            res += *cur_pos;
            while (res >> 16) res = (res & 0xFFFF) + (res >> 16);
            ++cur_pos;
        }
    }

    uint16_t check_res = (uint16_t)(res) + packet.header.checksum;

    return check_res == 0xFFFF;
    */

    uint16_t check_res = packet.header.checksum;
    genCheckSum(packet);

    // cerr << "Calculated checksum: " << packet.header.checksum << endl;
    // cerr << "Received checksum: " << check_res << endl;
    // cout << "Last char ascii: " << (uint8_t)packet.body[packet.header.data_len - 1] << endl;

    bool res               = (check_res == packet.header.checksum);
    packet.header.checksum = check_res;

    return res;
}