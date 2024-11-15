#ifndef __NET_RUDP_H__
#define __NET_RUDP_H__

#include <string>
#include <cstdint>
#include <chrono>
#include <net/socket_defs.h>

#define TIME_OUT_SCALER 2  // 2倍RTT无响应即超时
#define BUFF_MAX 10000

class UDPConnection
{
  private:
    int         socket_fd;
    sockaddr_in local_addr;
    sockaddr_in peer_addr;

    uint64_t cid;
    uint32_t seq_id;
    int32_t last_recv_seq_id;

    std::chrono::milliseconds rtt;

  public:
    UDPConnection(const std::string& local_ip, uint16_t local_port);
    ~UDPConnection();

    void reset();

    bool listen();
    bool connect(const std::string& peer_ip, uint16_t peer_port, uint8_t retry = 10);
    bool disconnect();
    bool wait_for_event(int socket_fd, bool is_read, uint32_t timeout_ms);


    bool     send(const char* data, uint32_t data_len, uint8_t retry = 10);
    uint32_t recv(char* data, uint32_t buff_size, uint8_t retry = 5);
};

#endif