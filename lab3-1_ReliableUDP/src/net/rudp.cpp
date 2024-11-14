#include <net/rudp.h>
#include <net/packet.h>
#include <net/socket_defs.h>
#include <random>
#include <cstring>
using namespace std;
using ms = chrono::milliseconds;

namespace
{
    static random_device                      rd;
    static uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
}  // namespace

UDPConnection::UDPConnection(const string& ip, uint16_t port)
    : target_ip(ip), port(port), cid(0), last_valid_pid(0), rtt(ms(0))
{}
UDPConnection::~UDPConnection()
{
    if (cid != 0) disconnect();
}

void UDPConnection::reset()
{
    if (cid == 0) return;

    disconnect();

    cid            = 0;
    last_valid_pid = 0;
    rtt            = ms(0);
}

void UDPConnection::set_ip(const string& ip) { this->target_ip = ip; }
void UDPConnection::set_port(uint16_t port) { this->port = port; }

bool listen()
{
    char connect_buffer[32];

    return true;
}

bool UDPConnection::connect(uint8_t retry)
{
    if (cid != 0) return false;

    cid = dist(rd);

    RUPacket syn_packet;
    syn_packet.header.flags |= SYN;
    syn_packet.header.cid = cid;
    set_sum_check(syn_packet);

    char connect_buffer[32];
    memcpy(connect_buffer, &syn_packet, sizeof(RUHeader));

    return true;
}

bool UDPConnection::disconnect() { return true; }