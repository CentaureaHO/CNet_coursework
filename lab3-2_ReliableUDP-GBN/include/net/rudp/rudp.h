#ifndef __NET_RUDP_RUDP_H__
#define __NET_RUDP_RUDP_H__

#include <net/socket_defs.h>
#include <net/nb_socket.h>
#include <net/rudp/rudp_defs.h>
#include <common/lock.h>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <deque>
#include <condition_variable>
#include <mutex>

void printRUDP(RUDP_P& p);

struct PacketTimer
{
    RUDP_P                    packet;
    std::chrono::microseconds timeout;
    int                       timeout_factor;

    template <typename P>
    PacketTimer(P&& p, std::chrono::microseconds t, int f) : packet(std::forward<P>(p)), timeout(t), timeout_factor(f)
    {}
};

class RUDP
{
  public:
    using Callback = std::function<void(RUDP_P&)>;

  private:
    RUDP_STATUS _statu;        // 当前状态
    sockaddr_in _local_addr;   // 本地地址
    sockaddr_in _remote_addr;  // 远程地址
    NBSocket    _nb_socket;    // 非阻塞套接字
    uint16_t    _local_port;   // 本地端口
    uint32_t    _connect_id;   // 连接ID

    uint32_t _seq_num;  // 序列号
    uint32_t _ack_num;  // 确认号

    std::chrono::microseconds _rtt;  // 往返时间

    std::map<uint32_t, PacketTimer> _send_buffer;     // 发送缓冲区，停等时只能在其中为空时添加新包
    std::deque<RUDP_P>              _receive_buffer;  // 接收缓冲区
    ReWrLock                        _buffer_lock;

    std::thread       _resend_thread;   // 重传线程
    std::atomic<bool> _resending;       // 是否正在重传
    std::thread       _receive_thread;  // 接收线程
    std::atomic<bool> _receiving;       // 是否正在接收

  public:
    RUDP(int local_port);
    ~RUDP();

  private:
    void _resendLoop();
    void _receiveHandler();
    void _server_receiveHandler(Callback cb);
    void clear_statu();

  public:
    int getBoundPort() const;

  public:
    void listen_listen();
    void listen_syn_rcvd();
    void listen_established(Callback cb);
    void listen_fin_wait();
    void listen_fin_rcvd();
    void listen_close_wait();
    void listen(Callback callback = printRUDP);

  public:
    bool connect(const char* remote_ip, int remote_port);
    bool disconnect();
    void send(const char* buffer, size_t buffer_size);
};

#endif