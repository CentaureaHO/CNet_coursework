#include <net/rudp/rudp.h>
#include <common/lock.h>
#include <random>
#include <iostream>
#include <thread>
#include <cassert>
using namespace std;

#define CONNECT_RETRY 10
#define GUESS_RTT 500000        // 假定rtt为500ms
#define QUERY_GAP 1000          // 1ms
#define SERVER_TIMEOUT 1000000  // 1s
#define FACTOR 2                // 以二倍rtt为超时时间
#define PATIENCE 10             // 重传或等待ack的次数

void printRUDP(RUDP_P& p)
{
    p.body[p.header.data_len] = '\0';
    cout << "connect_id: " << p.header.connect_id << endl;
    cout << "body: " << p.body << endl;
}

namespace
{
    random_device                      rd;
    mt19937                            gen(rd());
    uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
}  // namespace

using us = std::chrono::microseconds;

RUDP::RUDP(int local_port)
    : _statu(RUDP_STATUS::CLOSED), _socket(local_port), _seq_num(0), _last_ack(0), _rtt(GUESS_RTT)
{
    _local_port                 = _socket.getBoundPort();
    _local_addr.sin_family      = AF_INET;
    _local_addr.sin_port        = htons(_local_port);
    _local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
}
RUDP::~RUDP() {}

// common
int RUDP::getBoundPort() const { return _local_port; }

void RUDP::clear_statu()
{
    _statu    = RUDP_STATUS::CLOSED;
    _seq_num  = 0;
    _last_ack = 0;

    _remote_addr.sin_family      = AF_INET;
    _remote_addr.sin_port        = 0;
    _remote_addr.sin_addr.s_addr = 0;

    memset(&recv_buffer, 0, sizeof(recv_buffer));
    memset(&send_buffer, 0, sizeof(send_buffer));
}

// server
void RUDP::listen(Callback cb)
{
    clear_statu();
    _statu = RUDP_STATUS::LISTEN;

    while (true)
    {
        switch (_statu)
        {
            case RUDP_STATUS::CLOSED: return;
            case RUDP_STATUS::LISTEN: listen_listen(); break;
            case RUDP_STATUS::SYN_RCVD: listen_syn_rcvd(); break;
            case RUDP_STATUS::ESTABLISHED: listen_established(cb); break;
            case RUDP_STATUS::FIN_RCVD: listen_fin_rcvd(); break;
            case RUDP_STATUS::CLOSE_WAIT: listen_close_wait(); break;
            default: assert(false);
        }
    }
}

void RUDP::listen_listen()
{
    size_t received_length = 0;

    if (!_socket.recv(reinterpret_cast<char*>(&recv_buffer),
            sizeof(RUDP_H),
            &_remote_addr,
            received_length,
            us(SERVER_TIMEOUT),
            us(QUERY_GAP)))
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

    send_buffer.header.connect_id = _connect_id;
    send_buffer.header.seq_num    = _seq_num++;
    send_buffer.header.ack_num    = recv_buffer.header.seq_num + 1;
    SET_SYN(send_buffer);
    SET_ACK(send_buffer);
    genCheckSum(send_buffer);

    if (!_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr))
    {
        cout << "Failed to send SYN_ACK packet." << endl;
        return;
    }

    cout << "Sent SYN_ACK packet to " << inet_ntoa(_remote_addr.sin_addr) << ":" << ntohs(_remote_addr.sin_port)
         << endl;

    _statu = RUDP_STATUS::SYN_RCVD;
}

void RUDP::listen_syn_rcvd()
{
    size_t received_length = 0;

    for (int i = 0; i < PATIENCE; ++i)
    {
        if (!_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                sizeof(RUDP_H),
                &_remote_addr,
                received_length,
                us(SERVER_TIMEOUT),
                us(QUERY_GAP)))
        {
            cout << "Timeout waiting for ACK. Resending SYN_ACK..." << endl;
            _socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr);
            continue;
        }

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

        if (!CHK_ACK(recv_buffer))
        {
            cout << "Received a packet without ACK flag, drop it." << endl;
            continue;
        }

        if (recv_buffer.header.ack_num != _seq_num)
        {
            cout << "Received a packet with wrong ack number, drop it." << endl;
            continue;
        }

        cout << "Received an ACK packet, connection established." << endl;
        _statu = RUDP_STATUS::ESTABLISHED;
        return;
    }

    cout << "Failed to receive ACK packet, closing the connection." << endl;
    clear_statu();
    _statu = RUDP_STATUS::LISTEN;
}

void RUDP::listen_fin_rcvd()
{
    // Empty implementation, for now, you can add logic as needed
}

void RUDP::listen_close_wait()
{
    // Empty implementation, for now, you can add logic as needed
}

void RUDP::listen_established(Callback cb)
{
    size_t received_length = 0;

    while (true)
    {
        if (!_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                lenInByte(recv_buffer),
                &_remote_addr,
                received_length,
                us(SERVER_TIMEOUT),
                us(QUERY_GAP)))
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

        cb(recv_buffer);

        send_buffer.header.connect_id = _connect_id;
        send_buffer.header.seq_num    = _seq_num++;
        send_buffer.header.ack_num    = recv_buffer.header.seq_num + 1;
        SET_ACK(send_buffer);
        genCheckSum(send_buffer);

        if (!_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr))
        {
            cout << "Failed to send ACK packet." << endl;
            return;
        }
    }
}

// client
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

    send_buffer.header.connect_id = _connect_id;
    send_buffer.header.seq_num    = _seq_num++;
    SET_SYN(send_buffer);
    genCheckSum(send_buffer);

    chrono::high_resolution_clock::time_point start_time;
    chrono::high_resolution_clock::time_point end_time;

    size_t received_length = 0;

    for (int i = 0; i < PATIENCE; ++i)
    {
        if (!_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr))
        {
            cout << "Failed to send SYN packet." << endl;
            continue;
        }

        start_time = chrono::high_resolution_clock::now();

        if (!_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                sizeof(RUDP_H),
                &_remote_addr,
                received_length,
                _rtt * FACTOR,
                us(QUERY_GAP)))
        {
            cout << "Timeout waiting for SYN_ACK, retrying..." << endl;
            continue;
        }

        end_time = chrono::high_resolution_clock::now();

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

        if (!CHK_SYN(recv_buffer))
        {
            cout << "Received a packet without SYN flag, drop it." << endl;
            continue;
        }

        if (!CHK_ACK(recv_buffer))
        {
            cout << "Received a packet without ACK flag, drop it." << endl;
            continue;
        }

        if (recv_buffer.header.ack_num != _seq_num)
        {
            cout << "Received a packet with wrong ack number, drop it." << endl;
            continue;
        }

        cout << "Received a SYN_ACK packet, send ACK packet." << endl;
        _statu = RUDP_STATUS::SYN_SENT;
        break;
    }

    if (_statu != RUDP_STATUS::SYN_SENT)
    {
        cout << "Failed to connect to " << remote_ip << ":" << remote_port << endl;
        clear_statu();
        return false;
    }

    send_buffer.header.seq_num = _seq_num++;
    send_buffer.header.ack_num = recv_buffer.header.seq_num + 1;
    SET_ACK(send_buffer);
    genCheckSum(send_buffer);

    for (int i = 0; i < PATIENCE; ++i)
    {
        if (!_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr))
        {
            cout << "Failed to send ACK packet." << endl;
            continue;
        }

        // Wait for an optional response to confirm the connection is stable
        if (_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                sizeof(RUDP_H),
                &_remote_addr,
                received_length,
                _rtt * FACTOR,
                us(QUERY_GAP)))
        {
            if (checkCheckSum(recv_buffer) && recv_buffer.header.connect_id == _connect_id && CHK_ACK(recv_buffer))
            {
                cout << "Connection confirmed by server." << endl;
                break;
            }
        }
    }

    cout << "Sent ACK packet to " << remote_ip << ":" << remote_port << ", change status to ESTABLISHED." << endl;
    _statu = RUDP_STATUS::ESTABLISHED;
    CLR_FLAGS(send_buffer);

    _stop_receive   = false;
    _receive_thread = std::thread(&RUDP::receiveLoop, this);

    return true;
}

void RUDP::send(const char* buffer, size_t buffer_size)
{
    if (_statu != RUDP_STATUS::ESTABLISHED)
    {
        cout << "Connection not established." << endl;
        return;
    }

    send_buffer.header.seq_num  = _seq_num++;
    send_buffer.header.data_len = buffer_size;
    memcpy(send_buffer.body, buffer, buffer_size);
    genCheckSum(send_buffer);

    for (int i = 0; i < PATIENCE; ++i)
    {
        if (!_socket.send(reinterpret_cast<char*>(&send_buffer), lenInByte(send_buffer), &_remote_addr))
        {
            cout << "Failed to send data packet." << endl;
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 等待接收线程处理 ACK
        if (_last_ack >= send_buffer.header.seq_num)
        {
            cout << "ACK received for seq_num = " << send_buffer.header.seq_num << endl;
            return;
        }
    }

    cout << "Failed to send data packet after multiple attempts." << endl;
}

void RUDP::receiveLoop()
{
    size_t received_length = 0;

    while (!_stop_receive)
    {
        if (!_socket.recv(reinterpret_cast<char*>(&recv_buffer),
                sizeof(recv_buffer),
                &_remote_addr,
                received_length,
                _rtt * FACTOR,
                us(QUERY_GAP)))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

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
            cout << "Received ACK: seq_num = " << recv_buffer.header.ack_num << endl;
            _last_ack = recv_buffer.header.ack_num;
        }
        else if (CHK_FIN(recv_buffer))
        {
            cout << "Received FIN from server, connection closing." << endl;
            _statu = RUDP_STATUS::CLOSE_WAIT;
            break;
        }
        else { cout << "Received data: " << recv_buffer.body << endl; }
    }
}