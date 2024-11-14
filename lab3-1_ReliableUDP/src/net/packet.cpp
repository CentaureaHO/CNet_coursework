#include <cstring>
#include <iostream>
#include <net/packet.h>
#include <net/socket_defs.h>

uint16_t set_sum_check(RUPacket& packet)
{
    uint32_t  sum = 0;
    uint16_t* p   = (uint16_t*)&packet;

    uint16_t* end = (uint16_t*)(&packet.header.sum_check);

    while (p < end)
    {
        sum += *p;
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            ++sum;
        }
        ++p;
    }

    if (packet.data == nullptr)
    {
        packet.header.sum_check = ~sum;
        return ~sum;
    }

    uint32_t data_len = DATA_LENTH(packet.header.dlh, packet.header.dlm, packet.header.dll);
    char*    cp       = packet.data;
    char*    cend     = packet.data + data_len;

    while (cp < cend - 1)
    {
        sum += *((uint16_t*)cp);
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            ++sum;
        }
        cp += 2;
    }

    if (cp != cend)
    {
        sum += (uint16_t)(*cp) << 8;
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            ++sum;
        }
    }

    packet.header.sum_check = ~sum;
    return ~sum;
}

bool check_sum_check(const RUPacket& packet)
{
    uint32_t  sum = 0;
    uint16_t* p   = (uint16_t*)&packet;

    uint16_t* end = (uint16_t*)(&packet.header.reserved);

    while (p < end)
    {
        sum += *p;
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            ++sum;
        }
        ++p;
    }

    if (packet.data == nullptr) return sum == 0xFFFF;

    uint32_t data_len = DATA_LENTH(packet.header.dlh, packet.header.dlm, packet.header.dll);
    char*    cp       = packet.data;
    char*    cend     = packet.data + data_len;

    while (cp < cend - 1)
    {
        sum += *((uint16_t*)cp);
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            ++sum;
        }
        cp += 2;
    }

    if (cp != cend)
    {
        sum += (uint16_t)(*cp) << 8;
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            ++sum;
        }
    }

    return sum == 0xFFFF;
}