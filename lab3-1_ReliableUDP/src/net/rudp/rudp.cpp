#include <net/rudp/rudp.h>
#include <common/lock.h>
#include <random>
#include <iostream>
#include <thread>
#include <cassert>
using namespace std;

using us = chrono::microseconds;

#define GUESS_RTT 10000        // 初始时假定rtt为10ms
#define BASE_TIMEOUT_FACTOR 1  // 基础超时因子
#define TIMEOUT_FACTOR_INC 1   // 超时因子增量
#define MAX_TIMEOUT_FACTOR 10  // 最大超时因子
#define TICK_GAP 1000          // 每1ms检查一次超时

namespace
{
    random_device                      rd;
    mt19937                            gen(rd());
    uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
}  // namespace

const us tick_gap = us(TICK_GAP);

void printRUDP(RUDP_P& p)
{
    p.body[p.header.data_len] = '\0';
    cout << "connect_id: " << p.header.connect_id << endl;
    cout << "body: " << p.body << endl;
}

RUDP::RUDP(int local_port)
    : _statu(RUDP_STATUS::CLOSED), _nb_socket(local_port), _seq_num(0), _ack_num(0), _rtt(GUESS_RTT)
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
        {
            WriteGuard guard = _send_buffer_lock.write();
            for (auto& [seq_num, packet_timer] : _send_buffer)
            {
                packet_timer.timeout -= tick_gap;
                if (packet_timer.timeout.count() <= 0)
                {
                    if (packet_timer.timeout_factor < MAX_TIMEOUT_FACTOR)
                        packet_timer.timeout_factor += TIMEOUT_FACTOR_INC;
                    packet_timer.timeout = us(static_cast<long long>(_rtt.count() * packet_timer.timeout_factor));

                    _nb_socket.send((const char*)&packet_timer.packet, lenInByte(packet_timer.packet), &_remote_addr);
                    // cerr << "Resend packet " << packet_timer.packet.header.seq_num << endl;
                }
            }
        }

        this_thread::sleep_for(tick_gap);
    }
}

void RUDP::listen(Callback cb)
{
    _statu = RUDP_STATUS::LISTEN;

    while (true)
    {
        switch (_statu)
        {
            case RUDP_STATUS::CLOSED: return;
            case RUDP_STATUS::LISTEN: listen_listen(); break;
            case RUDP_STATUS::SYN_RCVD: listen_syn_rcvd(); break;
            case RUDP_STATUS::ESTABLISHED:
                listen_established(cb);
                break;
                /*case RUDP_STATUS::FIN_RCVD: listen_fin_rcvd(); break;
                case RUDP_STATUS::CLOSE_WAIT: listen_close_wait(); break;
                */
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
        cout << "Received a packet with wrong checksum, drop it." << endl;
        return;
    }

    if (!CHK_SYN(recv_buffer))
    {
        cout << "Received a packet without SYN flag, drop it." << endl;
        return;
    }

    if (CHK_ACK(recv_buffer))
    {
        cout << "Received a packet with ACK flag, drop it." << endl;
        return;
    }

    _connect_id = recv_buffer.header.connect_id;
    cout << "Received a SYN packet, connect_id: " << _connect_id << endl;

    RUDP_P send_buffer;
    send_buffer.header.connect_id = _connect_id;
    send_buffer.header.seq_num    = _seq_num++;
    send_buffer.header.ack_num    = recv_buffer.header.seq_num + 1;
    SET_SYN(send_buffer);
    SET_ACK(send_buffer);
    genCheckSum(send_buffer);

    if (!_nb_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr))
    {
        cout << "Failed to send SYN_ACK packet." << endl;
        return;
    }
    _send_buffer.emplace(send_buffer.header.seq_num,
        PacketTimer(send_buffer, us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)), BASE_TIMEOUT_FACTOR));

    cout << "Sent SYN_ACK packet to " << inet_ntoa(_remote_addr.sin_addr) << ":" << ntohs(_remote_addr.sin_port)
         << endl;

    _statu = RUDP_STATUS::SYN_RCVD;
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
            cout << "Received a packet with wrong checksum, drop it." << endl;
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            cout << "Received a packet with wrong connect_id, drop it." << endl;
            continue;
        }

        if (CHK_ACK(recv_buffer))
        {
            cout << "Received a ACK packet, connection established." << endl;
            _statu   = RUDP_STATUS::ESTABLISHED;
            _ack_num = recv_buffer.header.seq_num + 1;

            {
                WriteGuard guard = _send_buffer_lock.write();
                _send_buffer.erase(recv_buffer.header.seq_num);
            }
            break;
        }
    }
}

void RUDP::listen_established(Callback cb)
{
    size_t received_length = 0;
    RUDP_P recv_buffer;
    RUDP_P send_buffer;

    while (true)
    {
        if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                tick_gap))
            continue;

        /*
                if (!checkCheckSum(recv_buffer))
                {
                    cout << "Received a packet with wrong checksum, drop it." << endl;
                    continue;
                }
        */

        if (recv_buffer.header.connect_id != _connect_id)
        {
            cout << "Received a packet with wrong connect_id, drop it." << endl;
            continue;
        }

        if (recv_buffer.header.seq_num < _ack_num)
        {
            cout << "Received a old packet, send ACK packet." << endl;
            send_buffer.header.connect_id = _connect_id;
            send_buffer.header.seq_num    = _seq_num++;
            send_buffer.header.ack_num    = recv_buffer.header.seq_num + 1;
            SET_ACK(send_buffer);
            genCheckSum(send_buffer);

            _nb_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr);

            continue;
        }

        _ack_num = recv_buffer.header.seq_num + 1;
        cb(recv_buffer);

        send_buffer.header.connect_id = _connect_id;
        send_buffer.header.seq_num    = _seq_num++;
        send_buffer.header.ack_num    = recv_buffer.header.seq_num + 1;
        SET_ACK(send_buffer);
        genCheckSum(send_buffer);

        _nb_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr);
        _send_buffer.emplace(send_buffer.header.seq_num,
            PacketTimer(
                send_buffer, us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)), BASE_TIMEOUT_FACTOR));
    }
}

bool RUDP::connect(const char* remote_ip, int remote_port)
{
    if (_statu != RUDP_STATUS::CLOSED)
    {
        cout << "Connection already established." << endl;
        return false;
    }

    _remote_addr.sin_family      = AF_INET;
    _remote_addr.sin_port        = htons(remote_port);
    _remote_addr.sin_addr.s_addr = inet_addr(remote_ip);

    _connect_id                 = dist(gen);
    uint32_t syn_packet_seq_num = _seq_num++;

    RUDP_P syn_packet;
    syn_packet.header.connect_id = _connect_id;
    syn_packet.header.seq_num    = syn_packet_seq_num;
    SET_SYN(syn_packet);
    genCheckSum(syn_packet);
    _nb_socket.send((const char*)&syn_packet, lenInByte(syn_packet), &_remote_addr);
    _send_buffer.emplace(syn_packet_seq_num,
        PacketTimer(syn_packet, us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)), BASE_TIMEOUT_FACTOR));
    _resend_thread = thread(&RUDP::_resendLoop, this);

    _statu = RUDP_STATUS::SYN_SENT;

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
            cout << "Received a packet with wrong checksum, drop it." << endl;
            continue;
        }

        if (recv_packet.header.connect_id != _connect_id)
        {
            cout << "Received a packet with wrong connect_id, drop it." << endl;
            continue;
        }

        if (CHK_SYN(recv_packet) && CHK_ACK(recv_packet))
        {
            cout << "Received a SYN_ACK packet, send ACK packet, and enter ESTABLISHED state." << endl;
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

                _send_buffer.emplace(ack_packet.header.seq_num,
                    PacketTimer(ack_packet,
                        us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                        BASE_TIMEOUT_FACTOR));
                _nb_socket.send((const char*)&ack_packet, lenInByte(ack_packet), &_remote_addr);
            }

            break;
        }
    }

    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        cout << "Failed to connect to " << remote_ip << ":" << remote_port << endl;
        clear_statu();
        return false;
    }

    _receive_thread = thread(&RUDP::_receiveHandler, this);
    while (_send_buffer.size() != 0) this_thread::sleep_for(tick_gap);

    return true;
}

void RUDP::_receiveHandler()
{
    RUDP_P recv_buffer;
    size_t received_length = 0;

    while (true)
    {
        {
            {
                ReadGuard guard = _send_buffer_lock.read();
                if (_send_buffer.empty()) guard.~ReadGuard();
            }

            unique_lock<mutex> cv_lock(_send_buffer_cv_mtx);
            _send_buffer_cv.wait(cv_lock, [this] {
                ReadGuard read_guard = _send_buffer_lock.read();
                return !_send_buffer.empty();
            });
        }

        if (!_nb_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(static_cast<long long>(_rtt.count() * BASE_TIMEOUT_FACTOR)),
                tick_gap))
            continue;

        if (!checkCheckSum(recv_buffer))
        {
            cout << "Received a packet with wrong checksum, drop it." << endl;
            continue;
        }

        if (recv_buffer.header.connect_id != _connect_id)
        {
            cout << "Received a packet with wrong connect_id, drop it." << endl;
            continue;
        }

        {
            WriteGuard guard = _send_buffer_lock.write();
            _send_buffer.erase(recv_buffer.header.ack_num - 1);

            if (_send_buffer.empty())
            {
                lock_guard<mutex> cv_lock(_send_buffer_cv_mtx);
                _send_buffer_cv.notify_all();
            }
        }
    }
}

void RUDP::send(const char* buffer, size_t buffer_size)
{
    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        cout << "Connection not established." << endl;
        return;
    }

    RUDP_P packet;
    packet.header.connect_id = _connect_id;
    packet.header.seq_num    = _seq_num++;
    packet.header.data_len   = buffer_size;
    memcpy(packet.body, buffer, buffer_size);
    genCheckSum(packet);

    {
        WriteGuard guard = _send_buffer_lock.write();
        _send_buffer.emplace(packet.header.seq_num, PacketTimer(packet, _rtt, BASE_TIMEOUT_FACTOR));
    }

    _nb_socket.send((const char*)&packet, lenInByte(packet), &_remote_addr);

    {
        lock_guard<mutex> cv_lock(_send_buffer_cv_mtx);
        _send_buffer_cv.notify_all();
    }

    unique_lock<mutex> cv_lock(_send_buffer_cv_mtx);
    _send_buffer_cv.wait(cv_lock, [this] {
        ReadGuard guard = _send_buffer_lock.read();
        return _send_buffer.empty();
    });
}