#include <net/rudp/rudp.h>
#include <common/lock.h>
#include <random>
#include <iostream>
#include <thread>
#include <cassert>
#include <iomanip>
#include <common/log.h>
using namespace std;

#define SEND(rudp_packet)                                                                             \
    {                                                                                                 \
        sendto(_sockfd,                                                                               \
            (const char*)&rudp_packet,                                                                \
            lenInByte(rudp_packet),                                                                   \
            0,                                                                                        \
            (const struct sockaddr*)&_remote_addr,                                                    \
            sizeof(sockaddr_in));                                                                     \
        _send_buffer[rudp_packet.header.seq_num] = {                                                  \
            rudp_packet, chrono::time_point_cast<chrono::milliseconds>(chrono::steady_clock::now())}; \
    }

using ms = chrono::milliseconds;

namespace
{
    random_device                      rd;
    mt19937                            gen(rd());
    uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

    Logger client_log("client.log");
}  // namespace

#define CLOG(...) LOG(client_log, __VA_ARGS__)
#define CLOG_WARN(...) LOG_WARN(client_log, __VA_ARGS__)
#define CLOG_ERR(...) LOG_ERR(client_log, __VA_ARGS__)

RUDP_C::RUDP_C(int port, size_t /*w_s*/)
    : RUDP(port), _cwnd(1.0), _ssthresh(64.0), _fast_recovery(false), _ca_running(false), _base(0), _resending(false)
{}

RUDP_C::~RUDP_C()
{
    if (_sockfd != INVALID_SOCKET) CLOSE_SOCKET(_sockfd);
}

void RUDP_C::clear_statu()
{
    _statu   = RUDP_STATUS::CLOSED;
    _seq_num = 0;
    _ack_num = 0;

    _remote_addr.sin_family      = AF_INET;
    _remote_addr.sin_port        = 0;
    _remote_addr.sin_addr.s_addr = 0;

    if (_resending)
    {
        _resending = false;
        _resend_thread.join();
    }
    if (_receiving)
    {
        _receiving = false;

        sockaddr_in loopback_addr;
        ZeroMemory(&loopback_addr, sizeof(loopback_addr));
        loopback_addr.sin_family      = AF_INET;
        loopback_addr.sin_port        = htons(_port);
        loopback_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        sendto(_sockfd, "", 0, 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));
        _receive_thread.join();
    }
    _send_buffer.clear();
    _stop_congestion_avoidance_thread();
    _fast_recovery = false;
    _cwnd          = 1.0;
    _ssthresh      = 64.0;
}

void RUDP_C::_enter_slow_start()
{
    // 进入慢启动阶段
    _fast_recovery = false;
    _cwnd          = 1.0;
    CLOG("Enter Slow Start: cwnd=", _cwnd, ", ssthresh=", _ssthresh);
    _stop_congestion_avoidance_thread();
}

void RUDP_C::_enter_congestion_avoidance()
{
    // 进入拥塞避免阶段
    _fast_recovery = false;
    CLOG("Enter Congestion Avoidance: cwnd=", _cwnd, ", ssthresh=", _ssthresh);
    _start_congestion_avoidance_thread();
}

void RUDP_C::_enter_fast_recovery()
{
    // 进入快恢复阶段
    _fast_recovery = true;
    _ssthresh      = max(2.0, _cwnd / 2.0);
    _cwnd          = _ssthresh + 3.0;
    CLOG("Enter Fast Recovery: cwnd=", _cwnd, ", ssthresh=", _ssthresh);
    _stop_congestion_avoidance_thread();
}

void RUDP_C::_on_new_ack_in_fast_recovery()
{
    // 快恢复阶段收到新ACK，退出快恢复，进入拥塞避免
    _fast_recovery = false;
    _cwnd          = _ssthresh;
    CLOG("Exit Fast Recovery, into Congestion Avoidance: cwnd=", _cwnd, ", ssthresh=", _ssthresh);
    _enter_congestion_avoidance();
}

void RUDP_C::_on_timeout()
{
    // 超时处理：回到慢启动
    _ssthresh      = max(2.0, _cwnd / 2.0);
    _cwnd          = 1.0;
    _fast_recovery = false;
    CLOG_WARN("Timeout: cwnd=", _cwnd, ", ssthresh=", _ssthresh, ", enter Slow Start.");
    _stop_congestion_avoidance_thread();
}

void RUDP_C::_adjust_cwnd_on_ack(uint32_t acked_seq_diff)
{
    if (_fast_recovery)
    {
        _on_new_ack_in_fast_recovery();
        return;
    }

    if (_cwnd < _ssthresh)
    {
        _cwnd += acked_seq_diff;
        CLOG("Slow Start: cwnd=", _cwnd, ", ssthresh=", _ssthresh);
        if (_cwnd >= _ssthresh) _enter_congestion_avoidance();
    }
}

void RUDP_C::_start_congestion_avoidance_thread()
{
    if (!_ca_running)
    {
        _ca_running = true;
        _ca_thread  = thread(&RUDP_C::_congestion_avoidance_handler, this);
    }
}

void RUDP_C::_stop_congestion_avoidance_thread()
{
    if (_ca_running)
    {
        _ca_running = false;
        if (_ca_thread.joinable()) { _ca_thread.join(); }
    }
}

void RUDP_C::_congestion_avoidance_handler()
{
    while (_ca_running)
    {
        this_thread::sleep_for(_rtt);
        if (!_ca_running) break;
        if (_send_buffer.empty()) continue;
        if (!_fast_recovery && _statu == RUDP_STATUS::ESTABLISHED && _cwnd >= _ssthresh)
        {
            _cwnd += 1.0;
            CLOG("Congestion Avoidance increment: cwnd=", _cwnd, ", ssthresh=", _ssthresh);
        }
    }
}

void RUDP_C::_resend_handler()
{
    while (_resending)
    {
        {
            ms rto;
            {
                ReadGuard guard = _rto_lock.read();
                rto             = _rto;
            }

            WriteGuard guard = _send_buffer_lock.write();
            if (!_send_buffer.empty())
            {
                auto     now_ms       = chrono::time_point_cast<ms>(chrono::steady_clock::now());
                uint32_t seq_to_check = _base;
                auto     it           = _send_buffer.find(seq_to_check);
                if (it != _send_buffer.end())
                {
                    auto& [packet, last_send_time] = it->second;
                    if (now_ms - last_send_time > rto)
                    {
                        // 超时
                        _on_timeout();
                        CLOG_WARN("[",
                            statuStr(_statu),
                            "] Timeout at seq=",
                            seq_to_check,
                            ", resend all unacked packets starting from base.");

                        for (auto& [seq_num, ent] : _send_buffer)
                        {
                            auto& [p, t] = ent;
                            sendto(_sockfd,
                                (const char*)&p,
                                lenInByte(p),
                                0,
                                (const struct sockaddr*)&_remote_addr,
                                sizeof(sockaddr_in));
                            t = now_ms;  // 更新发送时间
                            CLOG("[", statuStr(_statu), "] Resend packet seq=", seq_num);
                        }
                    }
                }
            }
        }
        this_thread::sleep_for(check_gap);
    }
}

void RUDP_C::_receive_handler()
{
    RUDP_P      recv_buffer;
    socklen_t   addr_len = sizeof(sockaddr_in);
    sockaddr_in tmp_addr;
    size_t      cnt = 0;

    uint32_t last_ack_seq  = 0;
    int      dup_ack_count = 0;

    while (_receiving)
    {
        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);

        if (!_receiving) break;

        if (!checkCheckSum(recv_buffer))
        {
            CLOG_WARN("[", statuStr(_statu), "] Received corrupted packet (wrong checksum). Dropping.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            CLOG_WARN("[",
                statuStr(_statu),
                "] Received packet with unexpected connect_id=",
                recv_buffer.header.connect_id,
                ", expected=",
                _connect_id,
                ". Dropping.");
            continue;
        }

        CLOG("[",
            statuStr(_statu),
            "] Received packet: connect_id=",
            recv_buffer.header.connect_id,
            ", seq=",
            recv_buffer.header.seq_num,
            ", ack=",
            recv_buffer.header.ack_num,
            ", flags=",
            flagsToStr(recv_buffer),
            ", data_len=",
            recv_buffer.header.data_len);

        uint32_t acked_seq = recv_buffer.header.ack_num - 1;

        uint32_t acked_seq_diff = acked_seq - last_ack_seq;
        if (acked_seq_diff)
        {
            dup_ack_count = 0;
            last_ack_seq  = acked_seq;
        }
        else
            ++dup_ack_count;

        bool                 do_rtt_update = false;
        chrono::milliseconds sample_rtt(0);
        {
            WriteGuard guard = _send_buffer_lock.write();
            while (_base <= acked_seq && !_send_buffer.empty())
            {
                auto it = _send_buffer.find(_base);
                if (it != _send_buffer.end())
                {
                    if (++cnt % 5 == 0)
                    {
                        auto send_time = it->second.second;
                        auto now_ms    = chrono::time_point_cast<ms>(chrono::steady_clock::now());
                        sample_rtt     = now_ms - send_time;
                        do_rtt_update  = true;
                    }

                    _send_buffer.erase(it);
                }
                ++_base;
            }
        }

        if (do_rtt_update && sample_rtt.count() > 0)
        {
            auto err = chrono::duration_cast<ms>(sample_rtt - _rtt);
            _rtt += ms(static_cast<long>(_alpha * err.count()));
            auto abs_err = ms(abs(err.count()));
            _dev_rtt += ms(static_cast<long>(_beta * (abs_err.count() - _dev_rtt.count())));

            {
                WriteGuard guard = _rto_lock.write();
                _rto             = _rtt + 4 * _dev_rtt;
            }

            CLOG("[",
                statuStr(_statu),
                "] RTT sample: ",
                sample_rtt.count(),
                "ms, updated RTT=",
                _rtt.count(),
                "ms, DevRTT=",
                _dev_rtt.count(),
                "ms, RTO=",
                _rto.count(),
                "ms");
        }

        // 拥塞控制处理
        if (acked_seq_diff)
            _adjust_cwnd_on_ack(acked_seq_diff);
        else
        {
            if (dup_ack_count == 3)
            {
                CLOG_WARN(
                    "[", statuStr(_statu), "] 3 duplicate ACKs detected for ack_seq=", acked_seq, ", fast retransmit.");

                {
                    WriteGuard guard  = _send_buffer_lock.write();
                    auto       now_ms = chrono::time_point_cast<ms>(chrono::steady_clock::now());
                    for (auto& [seq_num, ent] : _send_buffer)
                    {
                        auto& [p, t] = ent;
                        sendto(_sockfd,
                            (const char*)&p,
                            lenInByte(p),
                            0,
                            (const struct sockaddr*)&_remote_addr,
                            sizeof(sockaddr_in));
                        t = now_ms;
                    }
                }

                _enter_fast_recovery();
            }
        }
    }
}

void RUDP_C::_wakeup_handler()
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
        this_thread::sleep_for(ms(2000));
        sendto(_sockfd, fake_c, len, 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));
    }
}

bool RUDP_C::connect(const char* remote_ip, int remote_port)
{
    if (_statu != RUDP_STATUS::CLOSED)
    {
        CLOG_ERR(" Connection already established.");
        return false;
    }

    _resending     = true;
    _resend_thread = thread(&RUDP_C::_resend_handler, this);
    CLOG(" Start resend thread.");

    _remote_addr.sin_family      = AF_INET;
    _remote_addr.sin_port        = htons(remote_port);
    _remote_addr.sin_addr.s_addr = inet_addr(remote_ip);

    _connect_id = dist(gen);
    CLOG(" Enter connect mode, generate connect_id=", _connect_id);

    RUDP_P syn_packet;
    syn_packet.header.connect_id = _connect_id;
    syn_packet.header.seq_num    = _seq_num++;
    SET_SYN(syn_packet);
    genCheckSum(syn_packet);
    {
        WriteGuard guard = _send_buffer_lock.write();
        SEND(syn_packet);
    }

    _statu = RUDP_STATUS::SYN_SENT;
    CLOG("[", statuStr(_statu), "] Send SYN packet to ", remote_ip, ":", remote_port, ". Change status to SYN_SENT.");

    RUDP_P      recv_packet;
    socklen_t   addr_len = sizeof(sockaddr_in);
    sockaddr_in tmp_addr;
    while (true)
    {
        recvfrom(_sockfd, (char*)&recv_packet, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);

        if (!checkCheckSum(recv_packet))
        {
            CLOG_WARN("[", statuStr(_statu), "] Received corrupted packet. Dropping.");
            continue;
        }

        if (recv_packet.header.connect_id != _connect_id)
        {
            CLOG_WARN("[", statuStr(_statu), "] Received packet with wrong connect_id. Dropping.");
            continue;
        }

        if (CHK_SYN(recv_packet) && CHK_ACK(recv_packet))
        {
            CLOG("[",
                statuStr(_statu),
                "] Received SYN_ACK packet: seq=",
                recv_packet.header.seq_num,
                ", ack=",
                recv_packet.header.ack_num,
                ". Change status to ESTABLISHED.");

            _statu = RUDP_STATUS::ESTABLISHED;
            RUDP_P ack_packet;
            ack_packet.header.connect_id = _connect_id;
            ack_packet.header.seq_num    = _seq_num++;
            ack_packet.header.ack_num    = recv_packet.header.seq_num + 1;
            SET_ACK(ack_packet);
            SET_SYN(ack_packet);
            genCheckSum(ack_packet);

            {
                WriteGuard guard = _send_buffer_lock.write();
                _send_buffer.erase(recv_packet.header.seq_num);
                SEND(ack_packet);
            }

            CLOG(" Now established, send ACK packet.");

            break;
        }
    }

    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        CLOG_ERR(" Failed to connect to ", remote_ip, ":", remote_port, ", revert to CLOSED.");
        return false;
    }

    _receiving      = true;
    _receive_thread = thread(&RUDP_C::_receive_handler, this);
    CLOG(" Start receive thread.");

    while (true)
    {
        this_thread::sleep_for(check_gap);
        {
            ReadGuard guard = _send_buffer_lock.read();
            if (_send_buffer.empty()) break;
        }
    }

    _base = _seq_num;
    // 初始进入慢启动
    _enter_slow_start();
    return true;
}

bool RUDP_C::disconnect()
{
    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        CLOG_ERR(" Connection not established.");
        return false;
    }

    // 停止拥塞控制线程
    _stop_congestion_avoidance_thread();

    while (true)
    {
        {
            ReadGuard guard = _send_buffer_lock.read();
            if (_send_buffer.empty()) break;
        }
        this_thread::sleep_for(check_gap);
    }

    _receiving            = false;
    SOCKET      send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in loopback_addr;
    ZeroMemory(&loopback_addr, sizeof(loopback_addr));
    loopback_addr.sin_family      = AF_INET;
    loopback_addr.sin_port        = htons(_port);
    loopback_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    string fake                   = "fake";
    cout << " Sent interrupt packet to stop receiving." << endl;
    sendto(send_sock, fake.c_str(), (int)fake.length(), 0, (const struct sockaddr*)&loopback_addr, sizeof(sockaddr_in));
    closesocket(send_sock);
    if (_receive_thread.joinable()) { _receive_thread.join(); }

    RUDP_P fin_packet;
    fin_packet.header.connect_id = _connect_id;
    fin_packet.header.seq_num    = _seq_num++;
    SET_FIN(fin_packet);
    genCheckSum(fin_packet);

    {
        WriteGuard guard = _send_buffer_lock.write();
        SEND(fin_packet);
    }
    CLOG("[",
        statuStr(_statu),
        "] Send FIN packet seq=",
        fin_packet.header.seq_num,
        " to ",
        inet_ntoa(_remote_addr.sin_addr),
        ". Change status to FIN_WAIT.");

    _statu = RUDP_STATUS::FIN_WAIT;

    RUDP_P      recv_buffer;
    socklen_t   addr_len = sizeof(sockaddr_in);
    sockaddr_in tmp_addr;
    while (true)
    {
        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);

        if (!checkCheckSum(recv_buffer))
        {
            CLOG_WARN("[", statuStr(_statu), "] Received corrupted packet during FIN_WAIT. Dropping.");
            continue;
        }
        if (recv_buffer.header.connect_id != _connect_id)
        {
            CLOG_WARN("[", statuStr(_statu), "] Wrong connect_id in FIN_WAIT. Dropping.");
            continue;
        }
        if (recv_buffer.header.ack_num != _seq_num)
        {
            CLOG_WARN("[", statuStr(_statu), "] Received packet with wrong ack_num during FIN_WAIT. Dropping.");
            continue;
        }
        if (!CHK_ACK(recv_buffer) || !CHK_FIN(recv_buffer))
        {
            CLOG_WARN("[", statuStr(_statu), "] Received packet without both ACK/FIN during FIN_WAIT. Dropping.");
            continue;
        }

        CLOG("[", statuStr(_statu), "] Received FIN_ACK packet seq=", recv_buffer.header.seq_num);

        _resending = false;
        if (_resend_thread.joinable()) _resend_thread.join();
        _send_buffer.erase(recv_buffer.header.ack_num - 1);

        CLR_FLAGS(fin_packet);
        fin_packet.header.seq_num = _seq_num++;
        fin_packet.header.ack_num = recv_buffer.header.seq_num + 1;
        SET_ACK(fin_packet);
        genCheckSum(fin_packet);

        sendto(_sockfd,
            (const char*)&fin_packet,
            lenInByte(fin_packet),
            0,
            (const struct sockaddr*)&_remote_addr,
            sizeof(sockaddr_in));
        _statu = RUDP_STATUS::CLOSE_WAIT;
        CLOG(" Change status to CLOSE_WAIT.");
        break;
    }

    if (_statu != RUDP_STATUS::CLOSE_WAIT)
    {
        cout << " Failed to disconnect properly." << endl;
        return false;
    }

    auto timeout    = ms(2000);  // 2s超时
    auto start_time = chrono::high_resolution_clock::now();
    auto now        = chrono::high_resolution_clock::now();
    _wakeup         = true;
    _wakeup_thread  = thread(&RUDP_C::_wakeup_handler, this);

    cout << " Wait for 2s to close connection." << endl;
    while (true)
    {
        now = chrono::high_resolution_clock::now();
        if (chrono::duration_cast<ms>(now - start_time) > timeout) break;

        recvfrom(_sockfd, (char*)&recv_buffer, sizeof(RUDP_P), 0, (struct sockaddr*)&tmp_addr, &addr_len);

        CLOG("[",
            statuStr(_statu),
            "] Receive packet at CLOSE_WAIT: connect_id=",
            recv_buffer.header.connect_id,
            ", seq=",
            recv_buffer.header.seq_num,
            ", ack=",
            recv_buffer.header.ack_num,
            ", flags=",
            flagsToStr(recv_buffer),
            ". Resend ACK packet ",
            fin_packet.header.seq_num);

        sendto(_sockfd,
            (const char*)&fin_packet,
            lenInByte(fin_packet),
            0,
            (const struct sockaddr*)&_remote_addr,
            sizeof(sockaddr_in));
    }

    _wakeup = false;
    if (_wakeup_thread.joinable()) _wakeup_thread.join();

    return true;
}

void RUDP_C::send(const char* buffer, size_t buffer_size)
{
    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        CLOG_ERR(" Connection not established.");
        return;
    }

    while (true)
    {
        {
            ReadGuard guard = _send_buffer_lock.read();
            // 使用cwnd控制发送窗口
            if (_seq_num < _base + current_window()) break;
        }
        this_thread::sleep_for(check_gap);
    }

    RUDP_P packet;
    packet.header.connect_id = _connect_id;
    packet.header.seq_num    = _seq_num++;
    packet.header.data_len   = buffer_size;
    memcpy(packet.body, buffer, buffer_size);
    genCheckSum(packet);

    {
        WriteGuard guard = _send_buffer_lock.write();
        SEND(packet);
    }

    CLOG("[",
        statuStr(_statu),
        "] Send packet: connect_id=",
        packet.header.connect_id,
        ", seq=",
        packet.header.seq_num,
        ", data_len=",
        packet.header.data_len,
        ", checksum=0x",
        hex,
        packet.header.checksum);
}