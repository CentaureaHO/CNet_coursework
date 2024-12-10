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

bool checkCheckSum(RUDP_P& packet)
{
    uint16_t received_checksum   = packet.header.checksum;
    uint16_t calculated_checksum = genCheckSum(packet);
    packet.header.checksum       = received_checksum;

    return received_checksum == calculated_checksum;
}

string statuStr(RUDP_STATUS statu)
{
    switch (statu)
    {
#define X(s, b, c) \
    case RUDP_STATUS::s: return #s;
        RUDP_STATU_LIST
#undef X
        default: return "UNKNOWN";
    }

    return "UNKNOWN";
}

std::string flagsToStr(const RUDP_P& p)
{
    std::string f;
    if (CHK_SYN(p)) f += "SYN ";
    if (CHK_ACK(p)) f += "ACK ";
    if (CHK_FIN(p)) f += "FIN ";
    if (f.empty()) f = "NONE";
    return f;
}

ostream& operator<<(ostream& os, const RUDP_H& header)
{
    os << "connect_id: " << header.connect_id << '\n'
       << "seq_num: " << header.seq_num << '\n'
       << "ack_num: " << header.ack_num << '\n'
       << "data_len: " << header.data_len << '\n'
       << "flags: 0x" << hex << header.flags << dec << " (";

    bool first = true;
    if (CHK_SYN_H(header))
    {
        os << (first ? "" : ", ") << "SYN";
        first = false;
    }
    if (CHK_ACK_H(header))
    {
        os << (first ? "" : ", ") << "ACK";
        first = false;
    }
    if (CHK_FIN_H(header))
    {
        os << (first ? "" : ", ") << "FIN";
        first = false;
    }
    if (CHK_RST_H(header))
    {
        os << (first ? "" : ", ") << "RST";
        first = false;
    }
    if (first) os << "NONE";

    os << ")\n"
       << "checksum: 0x" << hex << header.checksum << dec;
    return os;
}