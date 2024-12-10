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
    std::atomic<bool> _resending;
    std::thread       _resend_thread;

    std::map<uint32_t, entry> _send_buffer;
    ReWrLock                  _send_buffer_lock;

  public:
    RUDP_C(int port);
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