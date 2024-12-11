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

        sendto(
            _sockfd, fake.c_str(), (int)fake.length(), 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));
        _receive_thread.join();
    }
    if (_receive_thread.joinable()) _receive_thread.join();
}

void RUDP_S::_receive_handler(callback cb)
{
    size_t left_packet = 0;
    size_t cnt         = 0;
    RUDP_P send_buffer;

    map<uint32_t, RUDP_P> oOO_buffer;
    ReWrLock              oOO_lock;

    // 延迟ACK相关变量
    mutex                ack_mutex;
    condition_variable   ack_cv;
    bool                 ack_timer_running = false;
    bool                 ack_needed        = false;
    thread               ack_thread;
    bool                 stop_ack_thread = false;
    chrono::milliseconds ack_delay(10);  // 10ms延迟ACK时间
    uint32_t             last_answered_ack = 0;
    uint32_t             ack_times         = 0;

    auto ack_sender = [&]() {
        unique_lock<mutex> lk(ack_mutex);
        while (!stop_ack_thread)
        {
            if (!ack_needed)
            {
                // 没有需要发送的ACK，等待有新的ACK需要发送
                ack_cv.wait(lk, [&]() { return ack_needed || stop_ack_thread; });
            }
            else
            {
                // 已有需要发送的ACK，开始延迟计时
                auto deadline   = chrono::steady_clock::now() + ack_delay;
                bool no_new_ack = ack_cv.wait_until(lk, deadline, [&]() { return !ack_needed || stop_ack_thread; });
                // no_new_ack == false 表示超时了，没有新的包推动ack_needed清空
                if (!stop_ack_thread && ack_needed)
                {
                    // 超时未有新包到来，发送ACK
                    ack_needed = false;
                    // 发送ACK包
                    RUDP_P send_buffer;
                    send_buffer.header.connect_id = _connect_id;
                    send_buffer.header.seq_num    = _seq_num++;
                    send_buffer.header.ack_num    = _ack_num;
                    SET_ACK(send_buffer);
                    genCheckSum(send_buffer);
                    sendto(_sockfd,
                        (const char*)&send_buffer,
                        lenInByte(send_buffer),
                        0,
                        (const struct sockaddr*)&_remote_addr,
                        sizeof(sockaddr_in));
                    SLOG("[", statuStr(_statu), "] Delayed ACK sent: ack_num=", _ack_num);
                }
            }
        }
    };
    ack_thread = thread(ack_sender);

    auto trigger_ack = [&](bool immediate = false) {
        lock_guard<mutex> lk(ack_mutex);
        if (immediate)
        {
            ack_needed = false;
            RUDP_P send_buffer;
            send_buffer.header.connect_id = _connect_id;
            send_buffer.header.seq_num    = _seq_num++;
            send_buffer.header.ack_num    = _ack_num;
            SET_ACK(send_buffer);
            genCheckSum(send_buffer);
            sendto(_sockfd,
                (const char*)&send_buffer,
                lenInByte(send_buffer),
                0,
                (const struct sockaddr*)&_remote_addr,
                sizeof(sockaddr_in));
            SLOG("[", statuStr(_statu), "] Immediate ACK sent: ack_num=", _ack_num);
        }
        else if (!ack_needed)
        {
            ack_needed = true;
            ack_cv.notify_all();
        }
    };

    auto deliver_in_order = [&]() {
        WriteGuard guard(oOO_lock.write());
        while (true)
        {
            auto it = oOO_buffer.find(_ack_num);
            if (it != oOO_buffer.end())
            {
                RUDP_P& packet = it->second;
                SLOG("[",
                    statuStr(_statu),
                    "] Deliver queued packet seq=",
                    packet.header.seq_num,
                    ", now ack_num=",
                    _ack_num + 1);
                cb(packet);
                oOO_buffer.erase(it);
                ++_ack_num;
                ++cnt;
            }
            else
                break;
        }
    };

    while (true)
    {
        if (left_packet == 0)
        {
            {
                ReadGuard guard = _recv_queue_lock.read();
                left_packet     = _recv_queue.size();
            }
        }
        if (left_packet == 0)
        {
            this_thread::sleep_for(check_gap);
            continue;
        }

        --left_packet;
        RUDP_P recv_packet;
        {
            WriteGuard guard = _recv_queue_lock.write();
            recv_packet      = _recv_queue.front();
            _recv_queue.pop_front();
        }

        if (!checkCheckSum(recv_packet))
        {
            SLOG_WARN("[", statuStr(_statu), "] Received corrupted packet (wrong checksum). Dropping.");
            continue;
        }
        if (recv_packet.header.connect_id != _connect_id)
        {
            SLOG_WARN("[",
                statuStr(_statu),
                "] Received packet with unexpected connect_id=",
                recv_packet.header.connect_id,
                ", expected=",
                _connect_id,
                ". Dropping.");
            continue;
        }

        SLOG("[",
            statuStr(_statu),
            "] Received packet: connect_id=",
            recv_packet.header.connect_id,
            ", seq=",
            recv_packet.header.seq_num,
            ", ack=",
            recv_packet.header.ack_num,
            ", flags=",
            flagsToStr(recv_packet),
            ", data_len=",
            recv_packet.header.data_len);

        // FIN处理
        if (CHK_FIN(recv_packet))
        {
            SLOG("[",
                statuStr(_statu),
                "] Received FIN packet seq=",
                recv_packet.header.seq_num,
                ", ack_num=",
                recv_packet.header.ack_num,
                ". Prepare to close.");
            _ack_num   = recv_packet.header.seq_num + 1;
            _receiving = false;

            sockaddr_in loopback_addr;
            ZeroMemory(&loopback_addr, sizeof(loopback_addr));
            loopback_addr.sin_family      = AF_INET;
            loopback_addr.sin_port        = htons(_port);
            loopback_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            string      fake              = "fake";
            const char* fake_c            = fake.c_str();
            int         len               = (int)fake.length();
            sendto(_sockfd, fake_c, len, 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));

            break;
        }

        uint32_t seq_num = recv_packet.header.seq_num;

        if (seq_num < _ack_num)
        {
            // 老包，立即ACK
            SLOG("[",
                statuStr(_statu),
                "] Received old packet seq=",
                seq_num,
                " (current ack_num=",
                _ack_num,
                "), resend ACK immediately");
            trigger_ack(true);
        }
        else if (seq_num == _ack_num)
        {
            SLOG("[",
                statuStr(_statu),
                "] Received in-order packet seq=",
                seq_num,
                ". Deliver and ack_num=",
                _ack_num + 1);
            cb(recv_packet);
            ++_ack_num;
            deliver_in_order();
            trigger_ack(false);
        }
        else
        {
            {
                WriteGuard guard(oOO_lock.write());
                oOO_buffer.insert({seq_num, recv_packet});
            }

            SLOG("[",
                statuStr(_statu),
                "] Received out-of-order packet seq=",
                seq_num,
                " (expecting ",
                _ack_num,
                "), immediate ACK to signal sender.");
            trigger_ack(true);
        }

        memset(&recv_packet, 0, sizeof(RUDP_H));
    }

    {
        lock_guard<mutex> lk(ack_mutex);
        stop_ack_thread = true;
        ack_cv.notify_all();
    }
    ack_thread.join();
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
    int         len               = (int)fake.length();

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
            SLOG_WARN("[", statuStr(_statu), "] Received corrupted packet in LISTEN state. Dropping.");
            continue;
        }

        if (!CHK_SYN(recv_buffer))
        {
            SLOG_WARN("[", statuStr(_statu), "] Received packet without SYN in LISTEN state. Dropping.");
            continue;
        }

        if (CHK_ACK(recv_buffer))
        {
            SLOG_WARN("[", statuStr(_statu), "] Received ACK packet in LISTEN state. Dropping.");
            continue;
        }

        _connect_id = recv_buffer.header.connect_id;
        SLOG("[",
            statuStr(_statu),
            "] Received SYN packet: connect_id=",
            _connect_id,
            ", seq=",
            recv_buffer.header.seq_num,
            ". Sending SYN_ACK.");

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

        SLOG("[",
            statuStr(_statu),
            "] Sent SYN_ACK to ",
            inet_ntoa(_remote_addr.sin_addr),
            ":",
            ntohs(_remote_addr.sin_port));

        _statu = RUDP_STATUS::SYN_RCVD;
        SLOG(" Change status to SYN_RCVD.");
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
            SLOG_WARN("[", statuStr(_statu), "] Received corrupted packet in SYN_RCVD. Dropping.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            SLOG_WARN("[", statuStr(_statu), "] Wrong connect_id in SYN_RCVD. Dropping.");
            continue;
        }

        if (CHK_ACK(recv_buffer))
        {
            SLOG("[",
                statuStr(_statu),
                "] Received ACK packet: seq=",
                recv_buffer.header.seq_num,
                ", ack=",
                recv_buffer.header.ack_num,
                ". Connection established.");
            _statu   = RUDP_STATUS::ESTABLISHED;
            _ack_num = recv_buffer.header.seq_num + 1;

            SLOG(" Connection established, change status to ESTABLISHED.");

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
    SLOG(" Change status to FIN_RCVD.");
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
        SLOG("[",
            statuStr(_statu),
            "] Send FIN_ACK packet seq=",
            fin_ack_p.header.seq_num,
            ", ack=",
            fin_ack_p.header.ack_num);

        sendto(_sockfd,
            (const char*)&fin_ack_p,
            lenInByte(fin_ack_p),
            0,
            (const struct sockaddr*)&_remote_addr,
            sizeof(sockaddr_in));

        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);

        if (!checkCheckSum(recv_buffer))
        {
            SLOG_WARN("[", statuStr(_statu), "] Corrupted packet after FIN sent. Dropping.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            SLOG_WARN("[", statuStr(_statu), "] Wrong connect_id after FIN sent. Dropping.");
            continue;
        }

        if (CHK_ACK(recv_buffer) && !CHK_FIN(recv_buffer))
        {
            SLOG(
                "[", statuStr(_statu), "] Received last ACK packet seq=", recv_buffer.header.seq_num, ", final close.");
            _statu = RUDP_STATUS::CLOSED;
            SLOG(" Change status to CLOSED.");
            break;
        }
    }

    if (_statu != RUDP_STATUS::CLOSED)
    {
        SLOG_ERR(" Failed to receive last ACK packet, shutdown ungracefully.");
        _statu = RUDP_STATUS::CLOSED;
    }

    _wakeup_thread.join();
}

void RUDP_S::listen(callback cb)
{
    _statu = RUDP_STATUS::LISTEN;
    SLOG(" Enter listen mode, change status to LISTEN.");

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