#ifndef __NET_RUDP_H__
#define __NET_RUDP_H__

#include <string>
#include <cstdint>
#include <chrono>

#define TIME_OUT_SCALER 2  // 2倍RTT无响应即超时

class UDPConnection
{
  private:
    std::string target_ip;
    uint16_t    port;

    uint64_t cid;
    uint32_t last_valid_pid;

    std::chrono::milliseconds rtt;

  public:
    UDPConnection(const std::string& ip, uint16_t port);
    ~UDPConnection();

    void reset();

    void set_ip(const std::string& ip);
    void set_port(uint16_t port);

    bool connect(uint8_t retry = 5);
    bool disconnect();

    bool     send(const char* data, uint32_t data_len, uint8_t retry = 5);
    uint32_t recv(char* data, uint8_t retry = 5);
};

#endif