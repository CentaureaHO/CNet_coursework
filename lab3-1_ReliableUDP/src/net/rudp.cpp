#include <net/rudp.h>
#include <net/packet.h>
#include <random>
#include <iostream>
#include <cstring>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;
using ms = chrono::milliseconds;

namespace
{
    static random_device                      rd;
    static uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
}  // namespace

UDPConnection::UDPConnection(const std::string& local_ip, uint16_t local_port) : socket_fd(-1), cid(0), rtt(ms(0))
{

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    u_long mode = 1;
    if (ioctlsocket(socket_fd, FIONBIO, &mode) != 0)
    {
        perror("Failed to set non-blocking mode");
        CLOSE_SOCKET(socket_fd);
        exit(EXIT_FAILURE);
    }

    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = inet_addr(local_ip.c_str());
    local_addr.sin_port        = htons(local_port);

    if (::bind(socket_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0)
    {
        perror("Bind failed");
        CLOSE_SOCKET(socket_fd);
        exit(EXIT_FAILURE);
    }
}

UDPConnection::~UDPConnection()
{
    if (socket_fd >= 0) CLOSE_SOCKET(socket_fd);
}

void UDPConnection::reset()
{
    if (cid == 0) return;

    disconnect();

    cid = 0;
    rtt = ms(0);
}

bool UDPConnection::listen()
{
    char      listen_data[64];
    socklen_t peer_addr_len = sizeof(peer_addr);

    while (true)
    {
        int len =
            recvfrom(socket_fd, listen_data, sizeof(listen_data), 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
        if (len < 0)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) continue;
            perror("Data receive failed");
            return false;
        }

        RUPacket* syn_p = (RUPacket*)listen_data;
        if (!(syn_p->header.flags & SYN)) continue;
        if (syn_p->header.cid == 0) continue;
        cid = syn_p->header.cid;
        break;
    }

    RUPacket syn_ack_p;
    syn_ack_p.header.flags |= SYN | ACK;
    syn_ack_p.header.cid = cid;

    if (sendto(socket_fd, (char*)&syn_ack_p, HEADER_LEN, 0, (struct sockaddr*)&peer_addr, peer_addr_len) < 0)
    {
        perror("SYN ACK send failed");
        return false;
    }

    while (true)
    {
        int len =
            recvfrom(socket_fd, listen_data, sizeof(listen_data), 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
        if (len < 0)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) continue;
            perror("ACK receive failed");
            return false;
        }

        RUPacket* ack_p = (RUPacket*)listen_data;
        if (ack_p->header.cid != cid) continue;
        if (!(ack_p->header.flags & ACK)) continue;
        break;
    }

    return true;
}

bool UDPConnection::connect(const std::string& peer_ip, uint16_t peer_port, uint8_t retry)
{
    if (cid != 0) return false;
    cid = dist(rd);

    peer_addr.sin_family      = AF_INET;
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip.c_str());
    peer_addr.sin_port        = htons(peer_port);

    RUPacket syn_p;
    syn_p.header.flags |= SYN;
    syn_p.header.cid = cid;

    auto start_time = chrono::steady_clock::now();

    if (sendto(socket_fd, (char*)&syn_p, HEADER_LEN, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0)
    {
        perror("SYN send failed");
        return false;
    }

    char      recv_data[HEADER_LEN];
    socklen_t addr_len = sizeof(peer_addr);

    while (true)
    {
        int len = recvfrom(socket_fd, recv_data, HEADER_LEN, 0, (struct sockaddr*)&peer_addr, &addr_len);
        if (len < 0)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) continue;
            perror("SYN ACK receive failed");
            return false;
        }

        RUPacket* syn_ack_p = (RUPacket*)recv_data;
        if (syn_ack_p->header.cid != cid) continue;
        if (!(syn_ack_p->header.flags & SYN) || !(syn_ack_p->header.flags & ACK)) continue;

        auto end_time = chrono::steady_clock::now();
        rtt           = chrono::duration_cast<ms>(end_time - start_time);
        if (rtt < ms(5)) rtt = ms(5);

        cout << "Estimated RTT: " << rtt.count() << " ms" << endl;
        break;
    }

    RUPacket ack_p;
    ack_p.header.flags |= ACK;
    ack_p.header.cid = cid;
    if (sendto(socket_fd, (char*)&ack_p, HEADER_LEN, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0)
    {
        perror("ACK send failed");
        return false;
    }

    return true;
}

bool UDPConnection::disconnect() { return true; }

bool UDPConnection::send(const char* data, uint32_t data_len, uint8_t retry)
{
    socklen_t peer_addr_len = sizeof(peer_addr);

    RUPacket packet;
    packet.header.dlh = DATA_LENTH_H(data_len);
    packet.header.dlm = DATA_LENTH_M(data_len);
    packet.header.dll = DATA_LENTH_L(data_len);
    packet.header.cid = cid;
    packet.data       = new char[data_len];
    memcpy(packet.data, data, data_len);
    set_sum_check(packet);

    while (true)
    {
        if (sendto(socket_fd, (char*)&packet, HEADER_LEN + data_len, 0, (struct sockaddr*)&peer_addr, peer_addr_len) <
            0)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) continue;
            perror("Data send failed");
            delete[] packet.data;
            return false;
        }

        int len =
            recvfrom(socket_fd, (char*)&packet, HEADER_LEN + data_len, 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
        if (len < 0)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) continue;
            perror("Data receive failed");
            delete[] packet.data;
            return false;
        }

        RUPacket* ack_p = (RUPacket*)&packet;
        ack_p->data     = packet.data;
        if (ack_p->header.cid != cid) continue;
        if (!check_sum_check(*ack_p)) return this->send(packet.data, data_len);
        if (!(ack_p->header.flags & ACK)) return this->send(packet.data, data_len);
        break;
    }

    delete[] packet.data;
    return true;
}

uint32_t UDPConnection::recv(char* data, uint32_t buff_size, uint8_t retry)
{
    uint32_t  ret           = 0;
    socklen_t peer_addr_len = sizeof(peer_addr);

    while (true)
    {
        int len = recvfrom(socket_fd, data, buff_size, 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
        if (len == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) return 0;
            perror("Data receive failed");
            return 0;
        }

        RUPacket* packet = (RUPacket*)data;
        if (packet->header.cid != cid) continue;

        RUPacket ack_p;
        ack_p.header.flags |= ACK;
        ack_p.header.cid = cid;
        set_sum_check(ack_p);
        if (sendto(socket_fd, (char*)&ack_p, HEADER_LEN, 0, (struct sockaddr*)&peer_addr, peer_addr_len) < 0)
        {
            perror("ACK send failed");
            return 0;
        }

        ret             = DATA_LENTH(packet->header.dlh, packet->header.dlm, packet->header.dll);
        char* true_data = packet->data;
        memcpy(data, true_data, ret);
        break;
    }

    return ret;
}