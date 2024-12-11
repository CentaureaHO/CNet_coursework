#ifndef __NET_RUDP_RUDP_H__
#define __NET_RUDP_RUDP_H__

#include <net/socket_defs.h>
#include <net/rudp/rudp_defs.h>
#include <common/lock.h>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <condition_variable>
#include <mutex>
#include <deque>

#define GUESS_RTT 50
#define CHECK_GAP 10  // check timeout every 10ms
extern std::chrono::milliseconds check_gap;

void printRUDP(RUDP_P& p);

class RUDP
{
  protected:
    RUDP_STATUS _statu;
    int         _port;
    SOCKET      _sockfd;
    sockaddr_in _local_addr;
    sockaddr_in _remote_addr;

    uint32_t _connect_id;
    uint32_t _seq_num;
    uint32_t _ack_num;

    std::chrono::milliseconds _rtt;
    std::chrono::milliseconds _dev_rtt;
    double                    _alpha;
    double                    _beta;
    std::chrono::milliseconds _rto;
    ReWrLock                  _rto_lock;

    std::atomic<bool> _receiving;
    std::thread       _receive_thread;
    std::atomic<bool> _wakeup;
    std::thread       _wakeup_thread;

  public:
    RUDP(int port);
    virtual ~RUDP() = 0;

    int getBoundPort() const;

  protected:
    virtual void clear_statu() = 0;

  protected:
    virtual void _wakeup_handler() = 0;
};

class RUDP_C : public RUDP
{
  public:
    using entry = std::pair<RUDP_P, std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>>;

  private:
    double            _cwnd;           // 拥塞窗口(以报文段数计)
    double            _ssthresh;       // 慢启动阈值
    bool              _fast_recovery;  // 是否处于快恢复阶段
    std::atomic<bool> _ca_running;
    std::thread       _ca_thread;

    uint32_t          _base;
    std::atomic<bool> _resending;
    std::thread       _resend_thread;

    std::map<uint32_t, entry> _send_buffer;
    ReWrLock                  _send_buffer_lock;

  private:
    void _start_congestion_avoidance_thread();
    void _stop_congestion_avoidance_thread();
    void _congestion_avoidance_handler();

    void _enter_slow_start();
    void _enter_congestion_avoidance();
    void _enter_fast_recovery();
    void _on_new_ack_in_fast_recovery();
    void _on_timeout();

    void _adjust_cwnd_on_ack(uint32_t acked_seq_diff);

  public:
    RUDP_C(int port, size_t w_s = 20);
    virtual ~RUDP_C() override;

  private:
    virtual void clear_statu() override;
    void         _resend_handler();
    void         _receive_handler();
    virtual void _wakeup_handler() override;

  public:
    bool connect(const char* remote_ip, int remote_port);
    bool disconnect();
    void send(const char* buffer, size_t buffer_size);

    // 当前可用窗口大小（以整数方式返回）
    inline uint32_t current_window() { return static_cast<uint32_t>(_cwnd); }
};

class RUDP_S : public RUDP
{
  public:
    using callback = std::function<void(RUDP_P&)>;

  private:
    std::deque<RUDP_P> _recv_queue;
    ReWrLock           _recv_queue_lock;

  public:
    RUDP_S(int port);
    virtual ~RUDP_S() override;

  private:
    virtual void clear_statu() override;
    void         _receive_handler(callback cb);
    virtual void _wakeup_handler() override;

  private:
    void _listen();
    void _syn_rcvd();
    void _established(callback cb);
    void _fin_rcvd();

  public:
    void listen(callback cb = printRUDP);
};

#endif
