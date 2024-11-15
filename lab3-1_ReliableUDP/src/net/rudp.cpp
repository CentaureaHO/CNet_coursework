#include <net/rudp.h>
#include <net/packet.h>
#include <random>
#include <iostream>
#include <cstring>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cmath>
#include <algorithm>
#include <thread>

using namespace std;
using ms = chrono::milliseconds;

#define MAX_RETRIES 10

#define MAX_WAIT_TIME 1000
#define TIME_OUT_SCALER 2

namespace
{
    static random_device                      rd;
    static uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
}  // namespace

void print_winsock_error(const char* msg)
{
    int   error_code = WSAGetLastError();
    char* error_msg  = NULL;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&error_msg,
        0,
        NULL);

    cerr << msg << ": " << error_msg << endl;
    LocalFree(error_msg);
}

UDPConnection::UDPConnection(const string& local_ip, uint16_t local_port, bool non_block)
    : socket_fd(-1), cid(0), seq_id(0), last_seq_id_received(0), rtt(ms(1000))
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == INVALID_SOCKET)
    {
        print_winsock_error("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (non_block)
    {
        u_long mode = 1;
        if (ioctlsocket(socket_fd, FIONBIO, &mode) != 0)
        {
            print_winsock_error("Failed to set non-blocking mode");
            closesocket(socket_fd);
            exit(EXIT_FAILURE);
        }
    }

    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = inet_addr(local_ip.c_str());
    local_addr.sin_port        = htons(local_port);

    if (::bind(socket_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0)
    {
        print_winsock_error("Bind failed");
        closesocket(socket_fd);
        exit(EXIT_FAILURE);
    }
}

UDPConnection::~UDPConnection()
{
    if (socket_fd != INVALID_SOCKET) closesocket(socket_fd);
    WSACleanup();
}

void UDPConnection::reset()
{
    if (cid == 0) return;
    disconnect();
    cid = 0;
    rtt = ms(1000);
}

bool UDPConnection::connect(const string& peer_ip, uint16_t peer_port, uint8_t retry)
{
    seq_id = 0;
    cid    = dist(rd);

    peer_addr.sin_family      = AF_INET;
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip.c_str());
    peer_addr.sin_port        = htons(peer_port);

    RUPacket syn_p;
    syn_p.header.flags |= SYN;
    syn_p.header.cid    = cid;
    syn_p.header.seq_id = ++seq_id;
    set_sum_check(syn_p);

    cout << "Client: Sending SYN, CID = " << cid << ", SeqID = " << syn_p.header.seq_id << endl;

    bool syn_ack_received = false;

    for (uint8_t attempt = 0; attempt < retry; ++attempt)
    {
        if (sendto(socket_fd, (char*)&syn_p.header, HEADER_LEN, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0)
        {
            print_winsock_error("Client: Failed to send SYN");
            return false;
        }

        auto start = chrono::steady_clock::now();
        while (chrono::steady_clock::now() - start < ms(MAX_WAIT_TIME))
        {
            RUPacket syn_ack_p;
            int      addr_len = sizeof(peer_addr);
            int      len      = recvfrom(socket_fd,
                (char*)&syn_ack_p.header,
                sizeof(syn_ack_p.header),
                0,
                (struct sockaddr*)&peer_addr,
                &addr_len);
            if (len == HEADER_LEN)
            {
                if (syn_ack_p.header.flags & (SYN | ACK) && syn_ack_p.header.ack_id == syn_p.header.seq_id)
                {
                    cout << "Client: Received SYN-ACK, CID = " << syn_ack_p.header.cid
                         << ", SeqID = " << syn_ack_p.header.seq_id << ", AckID = " << syn_ack_p.header.ack_id << endl;

                    RUPacket ack_p;
                    ack_p.header.flags |= ACK;
                    ack_p.header.cid    = cid;
                    ack_p.header.seq_id = ++seq_id;
                    ack_p.header.ack_id = syn_ack_p.header.seq_id;
                    set_sum_check(ack_p);

                    if (sendto(socket_fd,
                            (char*)&ack_p.header,
                            HEADER_LEN,
                            0,
                            (struct sockaddr*)&peer_addr,
                            sizeof(peer_addr)) < 0)
                    {
                        print_winsock_error("Client: Failed to send ACK");
                        return false;
                    }
                    cout << "Client: Connection established" << endl;
                    syn_ack_received = true;
                    break;
                }
            }
            else if (len < 0)
            {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK)
                {
                    print_winsock_error("Client: Error receiving SYN-ACK");
                    return false;
                }
            }
        }

        if (syn_ack_received) { break; }
        else { cout << "Client: Timeout waiting for SYN-ACK, retrying..." << endl; }
    }

    if (!syn_ack_received)
    {
        cout << "Client: Failed to establish connection after retries" << endl;
        return false;
    }

    return true;
}

bool UDPConnection::listen()
{
    seq_id = 0;

    while (true)
    {
        RUPacket    syn_p;
        sockaddr_in client_addr;
        int         addr_len = sizeof(client_addr);
        int         len      = recvfrom(
            socket_fd, (char*)&syn_p.header, sizeof(syn_p.header), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (len == HEADER_LEN)
        {
            if (syn_p.header.flags & SYN)
            {
                cid = syn_p.header.cid;
                cout << "Server: Received SYN, CID = " << cid << ", SeqID = " << syn_p.header.seq_id << endl;

                RUPacket syn_ack_p;
                syn_ack_p.header.flags |= (SYN | ACK);
                syn_ack_p.header.cid    = cid;
                syn_ack_p.header.seq_id = ++seq_id;
                syn_ack_p.header.ack_id = syn_p.header.seq_id;
                set_sum_check(syn_ack_p);

                bool ack_received = false;

                for (uint8_t attempt = 0; attempt < MAX_RETRIES; ++attempt)
                {
                    if (sendto(socket_fd,
                            (char*)&syn_ack_p.header,
                            HEADER_LEN,
                            0,
                            (struct sockaddr*)&client_addr,
                            addr_len) < 0)
                    {
                        print_winsock_error("Server: Failed to send SYN-ACK");
                        return false;
                    }
                    cout << "Server: Sent SYN-ACK, CID = " << cid << ", SeqID = " << syn_ack_p.header.seq_id
                         << ", AckID = " << syn_ack_p.header.ack_id << endl;

                    auto start = chrono::steady_clock::now();
                    while (chrono::steady_clock::now() - start < ms(MAX_WAIT_TIME))
                    {
                        RUPacket ack_p;
                        len = recvfrom(socket_fd,
                            (char*)&ack_p.header,
                            sizeof(ack_p.header),
                            0,
                            (struct sockaddr*)&client_addr,
                            &addr_len);
                        if (len == HEADER_LEN)
                        {
                            if (ack_p.header.flags & ACK && ack_p.header.ack_id == syn_ack_p.header.seq_id)
                            {
                                cout << "Server: Received ACK, CID = " << ack_p.header.cid
                                     << ", SeqID = " << ack_p.header.seq_id << ", AckID = " << ack_p.header.ack_id
                                     << endl;
                                cout << "Server: Connection established" << endl;

                                peer_addr = client_addr;

                                ack_received = true;
                                break;
                            }
                        }
                        else if (len < 0)
                        {
                            int error = WSAGetLastError();
                            if (error != WSAEWOULDBLOCK)
                            {
                                print_winsock_error("Server: Error receiving ACK");
                                return false;
                            }
                        }
                    }

                    if (ack_received) { break; }
                    else { cout << "Server: Timeout waiting for ACK, retrying SYN-ACK..." << endl; }
                }

                if (!ack_received)
                {
                    cout << "Server: Failed to establish connection after retries" << endl;
                    return false;
                }

                return true;
            }
        }
        else if (len < 0)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                print_winsock_error("Server: Error receiving SYN");
                return false;
            }
        }
    }
    return false;
}

bool UDPConnection::disconnect()
{
    // Step 1: Send FIN
    RUPacket fin_p;
    fin_p.header.flags |= FIN;
    fin_p.header.cid    = cid;
    fin_p.header.seq_id = ++seq_id;
    set_sum_check(fin_p);

    cout << "Sending FIN, SeqID = " << fin_p.header.seq_id << endl;

    bool fin_ack_received = false;

    for (uint8_t attempt = 0; attempt < MAX_RETRIES; ++attempt)
    {
        if (sendto(socket_fd, (char*)&fin_p.header, HEADER_LEN, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0)
        {
            print_winsock_error("Failed to send FIN");
            return false;
        }

        // Wait for ACK of FIN
        auto start = chrono::steady_clock::now();
        while (chrono::steady_clock::now() - start < rtt * TIME_OUT_SCALER)
        {
            RUPacket    ack_p;
            sockaddr_in from_addr;
            int         addr_len = sizeof(from_addr);
            int         len      = recvfrom(
                socket_fd, (char*)&ack_p.header, sizeof(ack_p.header), 0, (struct sockaddr*)&from_addr, &addr_len);
            if (len == HEADER_LEN)
            {
                if (!check_sum_check(ack_p))
                {
                    cout << "Checksum mismatch in ACK, discarding" << endl;
                    continue;
                }

                if (ack_p.header.flags & ACK && ack_p.header.ack_id == fin_p.header.seq_id)
                {
                    fin_ack_received = true;
                    cout << "Received ACK for FIN, SeqID = " << ack_p.header.seq_id << endl;
                    break;
                }
            }
            else if (len < 0)
            {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK)
                {
                    print_winsock_error("Error receiving ACK for FIN");
                    return false;
                }
            }
        }

        if (fin_ack_received) { break; }
        else { cout << "Timeout waiting for ACK of FIN, retrying..." << endl; }
    }

    if (!fin_ack_received)
    {
        cout << "Failed to receive ACK for FIN after retries" << endl;
        return false;
    }

    // Step 2: Wait for FIN from peer
    bool fin_from_peer_received = false;
    auto start                  = chrono::steady_clock::now();
    while (chrono::steady_clock::now() - start < rtt * TIME_OUT_SCALER)
    {
        RUPacket    fin_p_peer;
        sockaddr_in from_addr;
        int         addr_len = sizeof(from_addr);
        int         len      = recvfrom(socket_fd,
            (char*)&fin_p_peer.header,
            sizeof(fin_p_peer.header),
            0,
            (struct sockaddr*)&from_addr,
            &addr_len);
        if (len == HEADER_LEN)
        {
            if (!check_sum_check(fin_p_peer))
            {
                cout << "Checksum mismatch in FIN from peer, discarding" << endl;
                continue;
            }

            if (fin_p_peer.header.flags & FIN)
            {
                fin_from_peer_received = true;
                cout << "Received FIN from peer, SeqID = " << fin_p_peer.header.seq_id << endl;

                // Step 3: Send ACK for peer's FIN
                RUPacket ack_p;
                ack_p.header.flags |= ACK;
                ack_p.header.cid    = cid;
                ack_p.header.seq_id = ++seq_id;
                ack_p.header.ack_id = fin_p_peer.header.seq_id;
                set_sum_check(ack_p);

                if (sendto(socket_fd,
                        (char*)&ack_p.header,
                        HEADER_LEN,
                        0,
                        (struct sockaddr*)&peer_addr,
                        sizeof(peer_addr)) < 0)
                {
                    print_winsock_error("Failed to send ACK for peer's FIN");
                    return false;
                }

                cout << "Sent ACK for peer's FIN, SeqID = " << ack_p.header.seq_id << endl;
                break;
            }
        }
        else if (len < 0)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                print_winsock_error("Error receiving FIN from peer");
                return false;
            }
        }
    }

    if (!fin_from_peer_received)
    {
        cout << "Failed to receive FIN from peer" << endl;
        return false;
    }

    this_thread::sleep_for(chrono::milliseconds(50));

    cout << "Disconnected successfully" << endl;
    cid = 0;
    return true;
}

bool UDPConnection::send(const char* data, uint32_t data_len, uint8_t retry)
{
    uint32_t data_sent = 0;

    while (data_sent < data_len)
    {
        uint32_t chunk_size = (data_len - data_sent > BUFF_MAX) ? BUFF_MAX : data_len - data_sent;

        RUPacket packet;
        packet.header.flags  = 0;
        packet.header.cid    = cid;
        packet.header.seq_id = ++seq_id;
        packet.header.dlh    = DATA_LENTH_H(chunk_size);
        packet.header.dlm    = DATA_LENTH_M(chunk_size);
        packet.header.dll    = DATA_LENTH_L(chunk_size);
        packet.data          = const_cast<char*>(data + data_sent);
        set_sum_check(packet);

        bool ack_received = false;

        for (uint8_t attempt = 0; attempt < retry; ++attempt)
        {
            char send_buffer[HEADER_LEN + BUFF_MAX];
            memcpy(send_buffer, &packet.header, HEADER_LEN);
            memcpy(send_buffer + HEADER_LEN, packet.data, chunk_size);

            int sent_len = sendto(
                socket_fd, send_buffer, HEADER_LEN + chunk_size, 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
            if (sent_len < 0)
            {
                print_winsock_error("Failed to send data");
                return false;
            }

            auto start = chrono::steady_clock::now();
            while (chrono::steady_clock::now() - start < rtt * TIME_OUT_SCALER)
            {
                RUPacket    ack_p;
                sockaddr_in from_addr;
                int         addr_len = sizeof(from_addr);
                int         len      = recvfrom(
                    socket_fd, (char*)&ack_p.header, sizeof(ack_p.header), 0, (struct sockaddr*)&from_addr, &addr_len);
                if (len == HEADER_LEN)
                {
                    if (!check_sum_check(ack_p))
                    {
                        cout << "Checksum mismatch in ACK, discarding" << endl;
                        continue;
                    }

                    if (ack_p.header.flags & ACK && ack_p.header.ack_id == seq_id)
                    {
                        ack_received = true;
                        break;
                    }
                }
                else if (len < 0)
                {
                    int error = WSAGetLastError();
                    if (error != WSAEWOULDBLOCK)
                    {
                        print_winsock_error("Error receiving ACK");
                        return false;
                    }
                }
            }

            if (ack_received)
            {
                data_sent += chunk_size;
                break;
            }
            else { cout << "Timeout waiting for ACK, retrying..." << endl; }
        }

        if (!ack_received)
        {
            cout << "Failed to send data after retries" << endl;
            return false;
        }
    }

    return true;
}

uint32_t UDPConnection::recv(char* data, uint32_t buff_size, uint8_t retry)
{
    RUPacket    packet;
    sockaddr_in from_addr;
    int         addr_len = sizeof(from_addr);

    auto start = chrono::steady_clock::now();

    while (true)
    {
        char recv_buffer[8192];
        int  len = recvfrom(socket_fd, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*)&from_addr, &addr_len);
        if (len >= HEADER_LEN)
        {
            memcpy(&packet.header, recv_buffer, len);

            uint32_t data_len = DATA_LENTH(packet.header.dlh, packet.header.dlm, packet.header.dll);

            if (packet.header.flags & FIN)
            {
                RUPacket ack_p;
                ack_p.header.flags |= ACK;
                ack_p.header.cid    = cid;
                ack_p.header.seq_id = ++seq_id;
                ack_p.header.ack_id = packet.header.seq_id;
                set_sum_check(ack_p);

                if (sendto(socket_fd, (char*)&ack_p.header, HEADER_LEN, 0, (struct sockaddr*)&from_addr, addr_len) < 0)
                {
                    print_winsock_error("Failed to send ACK for FIN");
                }

                cout << "Received FIN from peer, SeqID = " << packet.header.seq_id << endl;

                this_thread::sleep_for(chrono::milliseconds(50));

                RUPacket fin_p;
                fin_p.header.flags |= FIN;
                fin_p.header.cid    = cid;
                fin_p.header.seq_id = ++seq_id;
                set_sum_check(fin_p);

                if (sendto(socket_fd, (char*)&fin_p.header, HEADER_LEN, 0, (struct sockaddr*)&from_addr, addr_len) < 0)
                {
                    print_winsock_error("Failed to send FIN");
                }
                cout << "Sent FIN, SeqID = " << fin_p.header.seq_id << endl;

                // Wait for ACK of our FIN
                bool fin_ack_received = false;
                auto fin_start        = chrono::steady_clock::now();
                while (chrono::steady_clock::now() - fin_start < rtt * TIME_OUT_SCALER)
                {
                    RUPacket ack_p;
                    len = recvfrom(socket_fd,
                        (char*)&ack_p.header,
                        sizeof(ack_p.header),
                        0,
                        (struct sockaddr*)&from_addr,
                        &addr_len);
                    if (len == HEADER_LEN)
                    {
                        if (!check_sum_check(ack_p))
                        {
                            cout << "Checksum mismatch in ACK, discarding" << endl;
                            continue;
                        }

                        if (ack_p.header.flags & ACK && ack_p.header.ack_id == fin_p.header.seq_id)
                        {
                            fin_ack_received = true;
                            cout << "Received ACK for FIN, SeqID = " << ack_p.header.seq_id << endl;
                            break;
                        }
                    }
                    else if (len < 0)
                    {
                        int error = WSAGetLastError();
                        if (error != WSAEWOULDBLOCK)
                        {
                            print_winsock_error("Error receiving ACK for FIN");
                            return 0;
                        }
                    }
                }

                if (!fin_ack_received)
                {
                    cout << "Failed to receive ACK for our FIN" << endl;
                    return 0;
                }

                cout << "Connection closed gracefully" << endl;
                cid = 0;  // Reset connection ID
                return 0;
            }

            if (data_len > BUFF_MAX || data_len + HEADER_LEN > (uint32_t)len)
            {
                cout << "Data length exceeds buffer size or packet size mismatch" << endl;
                continue;
            }

            packet.data = new char[data_len];
            memcpy(packet.data, recv_buffer + HEADER_LEN, data_len);

            if (packet.header.seq_id <= last_seq_id_received)
            {
                // Resend ACK for already received packet
                RUPacket ack_p;
                ack_p.header.flags |= ACK;
                ack_p.header.cid    = cid;
                ack_p.header.seq_id = ++seq_id;
                ack_p.header.ack_id = packet.header.seq_id;
                set_sum_check(ack_p);

                sendto(socket_fd, (char*)&ack_p.header, HEADER_LEN, 0, (struct sockaddr*)&from_addr, addr_len);
                delete[] packet.data;
                continue;
            }

            last_seq_id_received = packet.header.seq_id;

            // Send ACK for received packet
            RUPacket ack_p;
            ack_p.header.flags |= ACK;
            ack_p.header.cid    = cid;
            ack_p.header.seq_id = ++seq_id;
            ack_p.header.ack_id = packet.header.seq_id;
            set_sum_check(ack_p);

            sendto(socket_fd, (char*)&ack_p.header, HEADER_LEN, 0, (struct sockaddr*)&from_addr, addr_len);

            uint32_t copy_size = min(data_len, buff_size);
            memcpy(data, packet.data, copy_size);
            delete[] packet.data;

            return copy_size;
        }
        else if (len < 0)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                print_winsock_error("Error receiving data");
                return 0;
            }
        }

        if (chrono::steady_clock::now() - start > rtt * TIME_OUT_SCALER)
        {
            cout << "Timeout waiting for data" << endl;
            return 0;
        }
    }
}