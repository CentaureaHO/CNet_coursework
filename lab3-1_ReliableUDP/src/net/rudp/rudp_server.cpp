#include <net/rudp/rudp.h>
#include <common/lock.h>
#include <random>
#include <iostream>
#include <thread>
#include <cassert>
#include <iomanip>
#include <common/log.h>
using namespace std;

using us = chrono::microseconds;
using ms = chrono::milliseconds;

namespace
{
    Logger server_log("server.log");
}  // namespace

#define SLOG(...) LOG(server_log, __VA_ARGS__)
#define SLOG_WARN(...) LOG_WARN(server_log, __VA_ARGS__)
#define SLOG_ERR(...) LOG_ERR(server_log, __VA_ARGS__)

RUDP_S::RUDP_S(int port) : RUDP(port) {}
RUDP_S::~RUDP_S()
{
    if (_sockfd != INVALID_SOCKET) CLOSE_SOCKET(_sockfd);
}

void RUDP_S::clear_statu()
{
    _statu   = RUDP_STATUS::CLOSED;
    _seq_num = 0;
    _ack_num = 0;

    _remote_addr.sin_family      = AF_INET;
    _remote_addr.sin_port        = 0;
    _remote_addr.sin_addr.s_addr = 0;

    if (_receiving)
    {
        _receiving = false;

        sockaddr_in loopback_addr;
        ZeroMemory(&loopback_addr, sizeof(loopback_addr));
        loopback_addr.sin_family      = AF_INET;
        loopback_addr.sin_port        = htons(_port);
        loopback_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        string fake = "fake";

        sendto(_sockfd, fake.c_str(), fake.length(), 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));
        _receive_thread.join();
    }
    if (_receive_thread.joinable()) _receive_thread.join();
}

void RUDP_S::_receive_handler(callback cb)
{
    size_t left_packet = 0;
    RUDP_P send_buffer;
    while (true)
    {
        if (left_packet == 0)
        {
            ReadGuard guard = _recv_queue_lock.read();
            left_packet     = _recv_queue.size();
        }
        if (left_packet == 0)
        {
            this_thread::sleep_for(check_gap);
            continue;
        }

        --left_packet;
        RUDP_P& recv_packet = _recv_queue.front();
        {
            WriteGuard guard = _recv_queue_lock.write();
            _recv_queue.pop_front();
        }

        if (!checkCheckSum(recv_packet))
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }
        if (recv_packet.header.connect_id != _connect_id)
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }
        if (recv_packet.header.seq_num < _ack_num)
        {
            SLOG(statuStr(_statu), ": Received a old packet, send ACK packet.");
            send_buffer.header.connect_id = _connect_id;
            send_buffer.header.seq_num    = _seq_num++;
            send_buffer.header.ack_num    = recv_packet.header.seq_num + 1;
            SET_ACK(send_buffer);
            genCheckSum(send_buffer);

            sendto(_sockfd,
                (const char*)&send_buffer,
                lenInByte(send_buffer),
                0,
                (const struct sockaddr*)&_remote_addr,
                sizeof(sockaddr_in));

            continue;
        }
        if (CHK_FIN(recv_packet))
        {
            SLOG(statuStr(_statu), ": Received a FIN packet:\n", recv_packet.header);
            _ack_num   = recv_packet.header.seq_num + 1;
            _receiving = false;

            sockaddr_in loopback_addr;
            ZeroMemory(&loopback_addr, sizeof(loopback_addr));
            loopback_addr.sin_family      = AF_INET;
            loopback_addr.sin_port        = htons(_port);
            loopback_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            string      fake              = "fake";
            const char* fake_c            = fake.c_str();
            int         len               = static_cast<int>(fake.length());
            sendto(_sockfd, fake_c, len, 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));

            break;
        }

        _ack_num = recv_packet.header.seq_num + 1;
        cb(recv_packet);

        send_buffer.header.connect_id = _connect_id;
        send_buffer.header.seq_num    = _seq_num++;
        send_buffer.header.ack_num    = recv_packet.header.seq_num + 1;
        SET_ACK(send_buffer);
        genCheckSum(send_buffer);
        sendto(_sockfd,
            (const char*)&send_buffer,
            lenInByte(send_buffer),
            0,
            (const struct sockaddr*)&_remote_addr,
            sizeof(sockaddr_in));

        memset(&recv_packet, 0, sizeof(RUDP_H));
    }
}
void RUDP_S::_wakeup_handler()
{
    sockaddr_in loopback_addr;
    ZeroMemory(&loopback_addr, sizeof(loopback_addr));
    loopback_addr.sin_family      = AF_INET;
    loopback_addr.sin_port        = htons(_port);
    loopback_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    string      fake              = "fake";
    const char* fake_c            = fake.c_str();
    int         len               = static_cast<int>(fake.length());

    while (_wakeup)
    {
        this_thread::sleep_for(ms(GUESS_RTT * 2));
        sendto(_sockfd, fake_c, len, 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));
    }
}

void RUDP_S::_listen()
{
    RUDP_P    recv_buffer;
    socklen_t addr_len = sizeof(sockaddr_in);

    while (true)
    {
        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&_remote_addr, &addr_len);

        if (!checkCheckSum(recv_buffer))
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (!CHK_SYN(recv_buffer))
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet without SYN flag, drop it.");
            continue;
        }

        if (CHK_ACK(recv_buffer))
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with ACK flag, drop it.");
            continue;
        }

        _connect_id = recv_buffer.header.connect_id;
        SLOG(statuStr(_statu), ": Received a SYN packet:\n", recv_buffer.header);

        RUDP_P send_buffer;
        send_buffer.header.connect_id = _connect_id;
        send_buffer.header.seq_num    = _seq_num++;
        send_buffer.header.ack_num    = recv_buffer.header.seq_num + 1;
        SET_SYN(send_buffer);
        SET_ACK(send_buffer);
        genCheckSum(send_buffer);

        sendto(_sockfd,
            (const char*)&send_buffer,
            lenInByte(send_buffer),
            0,
            (const struct sockaddr*)&_remote_addr,
            sizeof(sockaddr_in));

        SLOG(statuStr(_statu),
            ": Sent SYN_ACK packet to ",
            inet_ntoa(_remote_addr.sin_addr),
            ":",
            ntohs(_remote_addr.sin_port));

        _statu = RUDP_STATUS::SYN_RCVD;
        SLOG("Change statu to SYN_RCVD.");
        break;
    }
}
void RUDP_S::_syn_rcvd()
{
    RUDP_P      recv_buffer;
    sockaddr_in tmp_addr;
    socklen_t   addr_len = sizeof(sockaddr_in);

    while (true)
    {
        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);

        if (!checkCheckSum(recv_buffer))
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (CHK_ACK(recv_buffer))
        {
            SLOG(statuStr(_statu), ": Received a ACK packet:\n", recv_buffer.header);
            _statu   = RUDP_STATUS::ESTABLISHED;
            _ack_num = recv_buffer.header.seq_num + 1;
            SLOG("Connection established, change statu to ESTABLISHED.");

            RUDP_P ack_packet;
            ack_packet.header.connect_id = _connect_id;
            ack_packet.header.seq_num    = _seq_num++;
            ack_packet.header.ack_num    = recv_buffer.header.seq_num + 1;
            SET_ACK(ack_packet);
            genCheckSum(ack_packet);
            sendto(_sockfd,
                (const char*)&ack_packet,
                lenInByte(ack_packet),
                0,
                (const struct sockaddr*)&_remote_addr,
                sizeof(sockaddr_in));

            break;
        }
    }
}
void RUDP_S::_established(callback cb)
{
    RUDP_P      recv_buffer;
    sockaddr_in tmp_addr;
    socklen_t   addr_len = sizeof(sockaddr_in);

    _receiving      = true;
    _receive_thread = thread(&RUDP_S::_receive_handler, this, cb);
    do {
        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);
        {
            WriteGuard guard = _recv_queue_lock.write();
            _recv_queue.push_back(recv_buffer);
        }
    } while (_receiving);

    _receive_thread.join();

    _statu = RUDP_STATUS::FIN_RCVD;
    SLOG("Change statu to FIN_RCVD.");
}
void RUDP_S::_fin_rcvd()
{
    RUDP_P fin_ack_p;
    fin_ack_p.header.connect_id = _connect_id;
    fin_ack_p.header.seq_num    = _seq_num++;
    fin_ack_p.header.ack_num    = _ack_num;
    SET_ACK(fin_ack_p);
    SET_FIN(fin_ack_p);
    genCheckSum(fin_ack_p);

    RUDP_P      recv_buffer;
    sockaddr_in tmp_addr;
    socklen_t   addr_len = sizeof(sockaddr_in);

    bool flag      = true;
    _wakeup_thread = std::thread([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        flag = false;
    });

    while (flag)
    {
        sendto(_sockfd,
            (const char*)&fin_ack_p,
            lenInByte(fin_ack_p),
            0,
            (const struct sockaddr*)&_remote_addr,
            sizeof(sockaddr_in));

        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);

        if (!checkCheckSum(recv_buffer))
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (CHK_ACK(recv_buffer) && !CHK_FIN(recv_buffer))
        {
            SLOG(statuStr(_statu), ": Received last ACK packet\n", recv_buffer.header);
            _statu = RUDP_STATUS::CLOSED;
            SLOG("Change statu to CLOSED.");
            break;
        }
    }

    if (_statu != RUDP_STATUS::CLOSED)
    {
        SLOG_ERR("Failed to receive last ACK packet, shutdown ungracefully.");
        _statu = RUDP_STATUS::CLOSED;
    }

    _wakeup_thread.join();
}

void RUDP_S::listen(callback cb)
{
    _statu = RUDP_STATUS::LISTEN;
    SLOG("Enter listen mode, change statu to LISTEN.");

    while (true)
    {
        switch (_statu)
        {
            case RUDP_STATUS::CLOSED: return;
            case RUDP_STATUS::LISTEN: _listen(); break;
            case RUDP_STATUS::SYN_RCVD: _syn_rcvd(); break;
            case RUDP_STATUS::ESTABLISHED: _established(cb); break;
            case RUDP_STATUS::FIN_RCVD: _fin_rcvd(); break;
            default: assert(false);
        }
    }
}