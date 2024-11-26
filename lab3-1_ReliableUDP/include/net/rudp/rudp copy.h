#ifndef __NET_RUDP_RUDP_H__
#define __NET_RUDP_RUDP_H__

#include <net/socket_defs.h>
#include <net/nb_socket.h>
#include <net/rudp/rudp_defs.h>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>

void printRUDP(RUDP_P& p);

class RUDP
{
  public:
    using Callback = std::function<void(RUDP_P&)>;

  private:
    RUDP_STATUS _statu;
    sockaddr_in _local_addr;
    sockaddr_in _remote_addr;
    NBSocket    _socket;

    uint32_t _seq_num;
    uint32_t _last_ack;

    int _local_port;

    std::chrono::microseconds _rtt;

    uint32_t _connect_id;

    RUDP_P send_buffer;
    RUDP_P recv_buffer;

  private:
    std::thread       _receive_thread;  // 接收线程
    std::atomic<bool> _stop_receive;    // 控制接收线程的停止
    void              receiveLoop();    // 接收线程的主循环

  public:
    RUDP(int local_port);
    ~RUDP();

    // common
  public:
    int  getBoundPort() const;
    void clear_statu();

    // server
  private:
    void listen_listen();
    void listen_syn_rcvd();
    void listen_established(Callback cb);
    void listen_fin_wait();
    void listen_fin_rcvd();
    void listen_close_wait();

  public:
    void listen(Callback cb = printRUDP);

    // client
  public:
    bool connect(const char* remote_ip, int remote_port);
    void send(const char* buffer, size_t buffer_size);
    void stopReceiveThread();  // 停止接收线程
};

#endif