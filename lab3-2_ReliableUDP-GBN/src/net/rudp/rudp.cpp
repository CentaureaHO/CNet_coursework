#include <net/rudp/rudp.h>
#include <common/lock.h>
#include <random>
#include <iostream>
#include <thread>
#include <cassert>
#include <iomanip>
#include <unordered_map>
#include <common/log.h>
using namespace std;

using us = chrono::microseconds;

#define GUESS_RTT 30000        // 初始时假定rtt为100ms
#define BASE_TIMEOUT_FACTOR 2  // 基础超时因子
#define TIMEOUT_FACTOR_INC 1   // 超时因子增量
#define MAX_TIMEOUT_FACTOR 2  // 最大超时因子
#define TICK_GAP 5000          // 每5ms检查一次超时
#define PROCESS_GAP 0        // 给接收端20ms处理时间

namespace
{
    random_device                      rd;
    mt19937                            gen(rd());
    uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

    Logger server_log("server.log");
    Logger client_log("client.log");
}  // namespace

#define SLOG(...) LOG(server_log, __VA_ARGS__)
#define CLOG(...) LOG(client_log, __VA_ARGS__)

#define SLOG_WARN(...) LOG_WARN(server_log, __VA_ARGS__)
#define CLOG_WARN(...) LOG_WARN(client_log, __VA_ARGS__)

#define SLOG_ERR(...) LOG_ERR(server_log, __VA_ARGS__)
#define CLOG_ERR(...) LOG_ERR(client_log, __VA_ARGS__)

const us tick_gap    = us(TICK_GAP);
const us process_gap = us(PROCESS_GAP);

void printRUDP(RUDP_P& p)
{
    p.body[p.header.data_len] = '\0';
    cout << "connect_id: " << p.header.connect_id << endl;
    cout << "body: " << p.body << endl;
}

RUDP::RUDP(int local_port)
    : _statu(RUDP_STATUS::CLOSED),
      _nb_socket(local_port),
      _seq_num(0),
      _ack_num(0),
      _swindow_size(SENDER_WINDOW_SIZE),
      _window_base(0),
      _rtt(GUESS_RTT),
      _resending(false),
      _receiving(false)
{
    _local_port                 = _nb_socket.getBoundPort();
    _local_addr.sin_family      = AF_INET;
    _local_addr.sin_port        = htons(_local_port);
    _local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
}
RUDP::~RUDP() {}

int RUDP::getBoundPort() const { return _local_port; }

void RUDP::clear_statu()
{
    _statu   = RUDP_STATUS::CLOSED;
    _seq_num = 0;
    _ack_num = 0;

    _remote_addr.sin_family      = AF_INET;
    _remote_addr.sin_port        = 0;
    _remote_addr.sin_addr.s_addr = 0;
}

void RUDP::_resendLoop()
{
    while (true)
    {
        if (!_resending) break;

        {
            WriteGuard guard = _buffer_lock.write();
            for (auto& [seq_num, packet_timer] : _send_buffer)
            {
                packet_timer.timeout -= tick_gap;
                if (packet_timer.timeout.count() <= 0)
                {
                    if (packet_timer.timeout_factor < MAX_TIMEOUT_FACTOR)
                        packet_timer.timeout_factor += TIMEOUT_FACTOR_INC;
                    packet_timer.timeout = us(static_cast<long long>(_rtt.count() * packet_timer.timeout_factor));

                    _nb_socket.send((const char*)&packet_timer.packet, lenInByte(packet_timer.packet), &_remote_addr);
                    CLOG_WARN(statuStr(_statu),
                        ": No ack received for packet ",
                        packet_timer.packet.header.seq_num,
                        ", resend it.");

                    this_thread::sleep_for(process_gap);
                }
            }
        }

        this_thread::sleep_for(tick_gap);
    }
}

void RUDP::_receiveHandler()
{
    RUDP_P                  recv_buffer;
    size_t                  received_length = 0;
    unordered_map<int, int> ack_cnt_map;

    while (true)
    {
        if (!_receiving) break;

        if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                tick_gap))
            continue;

        if (!checkCheckSum(recv_buffer))
        {
            // cout << "Received a packet with wrong checksum, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            // cout << "Received a packet with wrong connect_id, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (recv_buffer.header.ack_num <= _window_base) continue;

        CLOG(statuStr(_statu), ": Received a packet:\n", recv_buffer.header);

        {
            WriteGuard guard = _buffer_lock.write();
            _window_base     = recv_buffer.header.ack_num;
            // _send_buffer.erase(recv_buffer.header.ack_num - 1);

            CLOG(statuStr(_statu),
                ": Received packet, ack_num: ",
                recv_buffer.header.ack_num,
                " remove all packets with seq_num less than it.");

            for (auto it = _send_buffer.begin(); it != _send_buffer.end();)
            {
                if (it->first < recv_buffer.header.ack_num)
                    it = _send_buffer.erase(it);
                else
                    ++it;
            }

            CLOG(statuStr(_statu), ": Fast resend all packets with seq_num less than ack_num.");
            for (auto& [seq_num, packet_timer] : _send_buffer)
            {
                packet_timer.timeout = us(static_cast<long long>(_rtt.count() * packet_timer.timeout_factor));
                _nb_socket.send((const char*)&packet_timer.packet, lenInByte(packet_timer.packet), &_remote_addr);
                CLOG(statuStr(_statu), ": Resend packet ", seq_num);
                this_thread::sleep_for(process_gap);
            }
        }
    }
}

void RUDP::_server_receiveHandler(Callback cb)
{
    RUDP_P                  recv_buffer;
    RUDP_P                  send_buffer;
    unordered_map<int, int> ack_cnt_map;

    int  ack_cnt        = 0;
    auto cur_time       = chrono::steady_clock::now();
    auto last_send_time = cur_time - chrono::seconds(1);

    while (true)
    {
        if (!_receiving) break;

        if (_receive_buffer.empty())
        {
            this_thread::sleep_for(tick_gap);
            continue;
        }

        {
            WriteGuard guard = _buffer_lock.write();
            recv_buffer      = _receive_buffer.front();
            _receive_buffer.pop_front();
        }

        if (!checkCheckSum(recv_buffer))
        {
            // cout << "Received a packet with wrong checksum, drop it." << endl;
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            // cout << "Received a packet with wrong connect_id, drop it." << endl;
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (recv_buffer.header.seq_num < _ack_num)
        {
            // cout << "Received a old packet, send ACK packet." << endl;
            SLOG(statuStr(_statu), ": Received a old packet, send newest ACK packet:\n", recv_buffer.header);
            send_buffer.header.connect_id = _connect_id;
            send_buffer.header.seq_num    = _seq_num++;
            send_buffer.header.ack_num    = _ack_num;
            SET_ACK(send_buffer);
            genCheckSum(send_buffer);

            _nb_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr);

            ack_cnt        = 0;
            last_send_time = chrono::steady_clock::now();

            continue;
        }
        else if (recv_buffer.header.seq_num > _ack_num)
        {
            // receive out-of-order packet, send ACK packet for the last in-order packet
            SLOG(statuStr(_statu), ": Received a out-of-order packet, send newest ACK packet:\n", recv_buffer.header);
            send_buffer.header.connect_id = _connect_id;
            send_buffer.header.seq_num    = _seq_num++;
            send_buffer.header.ack_num    = _ack_num;
            SET_ACK(send_buffer);
            genCheckSum(send_buffer);

            _nb_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr);

            ack_cnt        = 0;
            last_send_time = chrono::steady_clock::now();

            continue;
        }

        if (CHK_FIN(recv_buffer))
        {
            // cout << "Received a FIN packet, change statu to FIN_RCVD." << endl;
            SLOG(statuStr(_statu), ": Received a FIN packet:\n", recv_buffer.header);

            _statu = RUDP_STATUS::FIN_RCVD;
            SLOG("Change statu to FIN_RCVD.");

            _ack_num   = recv_buffer.header.seq_num + 1;
            _receiving = false;

            break;
        }

        SLOG(statuStr(_statu), ": Received a packet:\n", recv_buffer.header);

        _ack_num = recv_buffer.header.seq_num + 1;
        cb(recv_buffer);

        cur_time = chrono::steady_clock::now();
        ++ack_cnt;

        if (ack_cnt >= 5 || cur_time - last_send_time >= us(static_cast<long long>(_rtt.count() * 5)))
        {
            if (ack_cnt >= 5)
                SLOG(statuStr(_statu), ": Received 5 packets, send ACK packet.")
            else
                SLOG(statuStr(_statu), ": Timeout, send ACK packet.")

            ack_cnt        = 0;
            last_send_time = cur_time;

            send_buffer.header.connect_id = _connect_id;
            send_buffer.header.seq_num    = _seq_num++;
            send_buffer.header.ack_num    = _ack_num;
            SET_ACK(send_buffer);
            genCheckSum(send_buffer);

            _nb_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr);
        }
    }
}

void RUDP::listen(Callback cb)
{
    _statu = RUDP_STATUS::LISTEN;
    SLOG("Enter listen mode, change statu to LISTEN.");

    while (true)
    {
        switch (_statu)
        {
            case RUDP_STATUS::CLOSED: return;
            case RUDP_STATUS::LISTEN: listen_listen(); break;
            case RUDP_STATUS::SYN_RCVD: listen_syn_rcvd(); break;
            case RUDP_STATUS::ESTABLISHED: listen_established(cb); break;
            case RUDP_STATUS::FIN_RCVD: listen_fin_rcvd(); break;
            default: assert(false);
        }
    }
}

void RUDP::listen_listen()
{
    size_t received_length = 0;

    RUDP_P recv_buffer;
    if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
            sizeof(RUDP_H),
            &_remote_addr,
            received_length,
            us(5000000),
            us(10000)))
        return;

    if (!checkCheckSum(recv_buffer))
    {
        // cout << "Received a packet with wrong checksum, drop it." << endl;
        SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
        return;
    }

    if (!CHK_SYN(recv_buffer))
    {
        // cout << "Received a packet without SYN flag, drop it." << endl;
        SLOG_WARN(statuStr(_statu), ": Received a packet without SYN flag, drop it.");
        return;
    }

    if (CHK_ACK(recv_buffer))
    {
        // cout << "Received a packet with ACK flag, drop it." << endl;
        SLOG_WARN(statuStr(_statu), ": Received a packet with ACK flag, drop it.");
        return;
    }

    _connect_id = recv_buffer.header.connect_id;
    // cout << "Received a SYN packet, connect_id: " << _connect_id << endl;
    SLOG(statuStr(_statu), ": Received a SYN packet:\n", recv_buffer.header);

    RUDP_P send_buffer;
    send_buffer.header.connect_id = _connect_id;
    send_buffer.header.seq_num    = _seq_num++;
    send_buffer.header.ack_num    = recv_buffer.header.seq_num + 1;
    SET_SYN(send_buffer);
    SET_ACK(send_buffer);
    genCheckSum(send_buffer);

    if (!_nb_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr))
    {
        // cout << "Failed to send SYN_ACK packet." << endl;
        SLOG_ERR(statuStr(_statu), ": Failed to send SYN_ACK packet, turn back to LISTEN.");
        return;
    }
    _send_buffer.emplace(send_buffer.header.seq_num,
        PacketTimer(send_buffer, us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)), BASE_TIMEOUT_FACTOR));

    // cout << "Sent SYN_ACK packet to " << inet_ntoa(_remote_addr.sin_addr) << ":" << ntohs(_remote_addr.sin_port)
    //     << endl;
    SLOG(statuStr(_statu),
        ": Sent SYN_ACK packet to ",
        inet_ntoa(_remote_addr.sin_addr),
        ":",
        ntohs(_remote_addr.sin_port));

    _statu = RUDP_STATUS::SYN_RCVD;
    SLOG("Change statu to SYN_RCVD.");
}

void RUDP::listen_syn_rcvd()
{
    RUDP_P recv_buffer;
    size_t received_length = 0;

    while (true)
    {
        if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                sizeof(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                tick_gap))
            continue;

        if (!checkCheckSum(recv_buffer))
        {
            // cout << "Received a packet with wrong checksum, drop it." << endl;
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            // cout << "Received a packet with wrong connect_id, drop it." << endl;
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (CHK_ACK(recv_buffer))
        {
            // cout << "Received a ACK packet, connection established." << endl;
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

            _nb_socket.send(reinterpret_cast<char*>(&ack_packet), lenInByte(ack_packet), &_remote_addr);

            {
                WriteGuard guard = _buffer_lock.write();
                _send_buffer.erase(recv_buffer.header.seq_num);
            }
            break;
        }
    }
}

void RUDP::listen_established(Callback cb)
{
    _receiving      = true;
    _receive_thread = thread(&RUDP::_server_receiveHandler, this, cb);

    size_t received_length = 0;
    RUDP_P recv_buffer;
    RUDP_P send_buffer;

    while (true)
    {
        if (!_receiving) break;

        if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                us(10)))
            continue;

        {
            WriteGuard guard = _buffer_lock.write();
            _receive_buffer.push_back(recv_buffer);
        }

        continue;
    }

    if (_receive_thread.joinable()) _receive_thread.join();
}

void RUDP::listen_fin_rcvd()
{
    RUDP_P fin_ack_p;
    fin_ack_p.header.connect_id = _connect_id;
    fin_ack_p.header.seq_num    = _seq_num++;
    fin_ack_p.header.ack_num    = _ack_num;
    SET_ACK(fin_ack_p);
    SET_FIN(fin_ack_p);
    genCheckSum(fin_ack_p);

    RUDP_P recv_buffer;
    size_t received_length = 0;

    // cout << "Waiting for last ACK packet." << endl;

    auto max_wait_time = chrono::milliseconds(10000);
    auto start_time    = chrono::steady_clock::now();

    while (true)
    {
        auto current_time = chrono::steady_clock::now();
        auto elapsed_time = duration_cast<chrono::milliseconds>(current_time - start_time);
        if (elapsed_time >= max_wait_time)
        {
            // cout << "Timeout: No ACK received within the specified time. Connection closed." << endl;
            SLOG_ERR(statuStr(_statu), ": Timeout: No ACK received within the specified time. Connection closed.");
            _statu = RUDP_STATUS::CLOSED;
            break;
        }

        _nb_socket.send(reinterpret_cast<char*>(&fin_ack_p), lenInByte(fin_ack_p), &_remote_addr);

        if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                tick_gap))
            continue;

        if (!checkCheckSum(recv_buffer))
        {
            // cout << "Received a packet with wrong checksum, drop it." << endl;
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            // cout << "Received a packet with wrong connect_id, drop it." << endl;
            SLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (CHK_ACK(recv_buffer) && !CHK_FIN(recv_buffer))
        {
            // cout << "Received last ACK packet, connection closed." << endl;
            SLOG(statuStr(_statu), ": Received last ACK packet", recv_buffer.header);
            _statu = RUDP_STATUS::CLOSED;
            SLOG("Change statu to CLOSED.");
            break;
        }
    }
}

bool RUDP::connect(const char* remote_ip, int remote_port)
{
    if (_statu != RUDP_STATUS::CLOSED)
    {
        // cout << "Connection already established." << endl;
        CLOG_ERR("Connection already established.");
        return false;
    }

    _remote_addr.sin_family      = AF_INET;
    _remote_addr.sin_port        = htons(remote_port);
    _remote_addr.sin_addr.s_addr = inet_addr(remote_ip);

    _connect_id = dist(gen);
    CLOG("Enter connect mode and generate connect_id: ", _connect_id);

    uint32_t syn_packet_seq_num = _seq_num++;

    RUDP_P syn_packet;
    syn_packet.header.connect_id = _connect_id;
    syn_packet.header.seq_num    = syn_packet_seq_num;
    SET_SYN(syn_packet);
    genCheckSum(syn_packet);
    _nb_socket.send((const char*)&syn_packet, lenInByte(syn_packet), &_remote_addr);
    _send_buffer.emplace(syn_packet_seq_num,
        PacketTimer(syn_packet, us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)), BASE_TIMEOUT_FACTOR));

    _resending     = true;
    _resend_thread = thread(&RUDP::_resendLoop, this);

    CLOG(statuStr(_statu), ": Send SYN packet to ", remote_ip, ":", remote_port);
    _statu = RUDP_STATUS::SYN_SENT;
    CLOG("Change statu to SYN_SENT.");

    RUDP_P recv_packet;
    size_t received_length = 0;
    while (true)
    {
        if (!_nb_socket.recv((char*)&recv_packet,
                lenInByte(recv_packet),
                &_remote_addr,
                received_length,
                us(_rtt.count() * BASE_TIMEOUT_FACTOR),
                tick_gap))
            continue;

        if (!checkCheckSum(recv_packet))
        {
            // cout << "Received a packet with wrong checksum, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_packet.header.connect_id != _connect_id)
        {
            // cout << "Received a packet with wrong connect_id, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (CHK_SYN(recv_packet) && CHK_ACK(recv_packet))
        {
            // cout << "Received a SYN_ACK packet, send ACK packet, and enter ESTABLISHED state." << endl;
            CLOG(statuStr(_statu), ": Received a SYN_ACK packet:\n", recv_packet.header);
            _statu = RUDP_STATUS::ESTABLISHED;
            RUDP_P ack_packet;
            ack_packet.header.connect_id = _connect_id;
            ack_packet.header.seq_num    = _seq_num++;
            ack_packet.header.ack_num    = recv_packet.header.seq_num + 1;
            SET_ACK(ack_packet);
            SET_SYN(ack_packet);
            genCheckSum(ack_packet);

            {
                WriteGuard guard = _buffer_lock.write();
                _send_buffer.erase(recv_packet.header.seq_num);

                _send_buffer.emplace(ack_packet.header.seq_num,
                    PacketTimer(ack_packet,
                        us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                        BASE_TIMEOUT_FACTOR));
                _nb_socket.send((const char*)&ack_packet, lenInByte(ack_packet), &_remote_addr);
            }

            CLOG("Change statu to ESTABLISHED, and send ACK packet.");

            break;
        }
    }

    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        // cout << "Failed to connect to " << remote_ip << ":" << remote_port << endl;
        CLOG_ERR("Failed to connect to ", remote_ip, ":", remote_port, ", turn back to CLOSED.");
        clear_statu();
        return false;
    }

    _window_base    = _seq_num - 1;
    _receiving      = true;
    _receive_thread = thread(&RUDP::_receiveHandler, this);
    while (_send_buffer.size() != 0) this_thread::sleep_for(tick_gap);

    return true;
}

void RUDP::send(const char* buffer, size_t buffer_size)
{
    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        // cout << "Connection not established." << endl;
        CLOG_ERR("Connection not established.");
        return;
    }

    RUDP_P packet;
    packet.header.connect_id = _connect_id;
    packet.header.seq_num    = _seq_num++;
    packet.header.data_len   = buffer_size;
    memcpy(packet.body, buffer, buffer_size);
    genCheckSum(packet);

    while (_send_buffer.size() >= _swindow_size) this_thread::sleep_for(tick_gap);

    {
        WriteGuard guard = _buffer_lock.write();
        _send_buffer.emplace(packet.header.seq_num,
            PacketTimer(packet, us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)), BASE_TIMEOUT_FACTOR));
    }

    _nb_socket.send((const char*)&packet, lenInByte(packet), &_remote_addr);
    CLOG(statuStr(_statu),
        ": Send packet ",
        packet.header.seq_num,
        " to ",
        inet_ntoa(_remote_addr.sin_addr),
        " with checksum 0x",
        hex,
        packet.header.checksum);

    this_thread::sleep_for(process_gap);
}

bool RUDP::disconnect()
{
    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        // cout << "Connection not established." << endl;
        CLOG_ERR("Connection not established.");
        return false;
    }

    while (_send_buffer.size() > 0) this_thread::sleep_for(tick_gap);

    // cout << "Disconnecting..." << endl;
    {
        _receiving = false;
        if (_receive_thread.joinable()) _receive_thread.join();
    }
    // cout << "Receive thread stopped." << endl;

    RUDP_P fin_packet;
    fin_packet.header.connect_id = _connect_id;
    fin_packet.header.seq_num    = _seq_num++;
    SET_FIN(fin_packet);
    genCheckSum(fin_packet);

    _nb_socket.send((const char*)&fin_packet, lenInByte(fin_packet), &_remote_addr);
    _send_buffer.emplace(fin_packet.header.seq_num, PacketTimer(fin_packet, _rtt, BASE_TIMEOUT_FACTOR));
    CLOG(statuStr(_statu), ": Send FIN packet ", fin_packet.header.seq_num, " to ", inet_ntoa(_remote_addr.sin_addr));

    _statu = RUDP_STATUS::FIN_WAIT;
    CLOG("Change statu to FIN_WAIT.");

    RUDP_P recv_buffer;
    size_t received_length = 0;

    auto start         = chrono::steady_clock::now();
    auto end           = chrono::steady_clock::now();
    auto max_wait_time = chrono::seconds(10);

    while (chrono::duration_cast<chrono::seconds>(end - start) < max_wait_time)
    {
        end = chrono::steady_clock::now();

        if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                tick_gap))
            continue;

        if (!checkCheckSum(recv_buffer))
        {
            // cout << "Received a packet with wrong checksum, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet with wrong checksum, drop it.");
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            // cout << "Received a packet with wrong connect_id, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet with wrong connect_id, drop it.");
            continue;
        }

        if (recv_buffer.header.ack_num != _seq_num)
        {
            // cout << "Received a packet with wrong ack_num, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet with wrong ack_num, drop it.");
            continue;
        }

        if (!CHK_ACK(recv_buffer) || !CHK_FIN(recv_buffer))
        {
            // cout << "Received a packet without ACK/FIN flag, drop it." << endl;
            CLOG_WARN(statuStr(_statu), ": Received a packet without ACK/FIN flag, drop it.");
            continue;
        }

        CLOG(statuStr(_statu), ": Received FIN_ACK packet ", recv_buffer.header.seq_num);

        {
            WriteGuard guard = _buffer_lock.write();
            _send_buffer.erase(recv_buffer.header.ack_num - 1);

            _resending = false;
            if (_resend_thread.joinable()) _resend_thread.join();
        }

        // CLOG(statuStr(_statu), ": Received FIN_ACK packet ", recv_buffer.header.seq_num);

        CLR_FLAGS(fin_packet);
        fin_packet.header.seq_num = _seq_num++;
        fin_packet.header.ack_num = recv_buffer.header.seq_num + 1;
        SET_ACK(fin_packet);
        genCheckSum(fin_packet);

        _nb_socket.send((const char*)&fin_packet, lenInByte(fin_packet), &_remote_addr);

        _statu = RUDP_STATUS::CLOSE_WAIT;
        CLOG("Change statu to CLOSE_WAIT.");
        break;
    }

    if (_statu != RUDP_STATUS::CLOSE_WAIT)
    {
        cout << "Failed to disconnect." << endl;
        return false;
    }

    start = chrono::steady_clock::now();
    end   = chrono::steady_clock::now();

    while (chrono::duration_cast<chrono::seconds>(end - start).count() < 2)
    {
        this_thread::sleep_for(tick_gap);

        if (_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(2000000)),
                tick_gap))
        {
            _nb_socket.send((const char*)&fin_packet, lenInByte(fin_packet), &_remote_addr);
            CLOG(statuStr(_statu), ": Receive packet at CLOSE_WAIT:\n", recv_buffer.header);
            CLOG(statuStr(_statu), ": Resend ACK packet ", fin_packet.header.seq_num);
        }

        end = chrono::steady_clock::now();
    }

    clear_statu();

    return true;
}