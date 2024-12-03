#ifndef __NET_NB_SOCKET_H
#define __NET_NB_SOCKET_H

#include <net/socket_defs.h>
#include <chrono>

class NBSocket
{
  private:
    int      _port;
    Protocol _protocol;
    SOCKET   _sockfd;

  public:
    NBSocket(int port, Protocol protocol = Protocol::UDP);
    ~NBSocket();

  public:
    bool init();

    int getBoundPort() const;

    bool send(const char* buffer, size_t buffer_size, const sockaddr_in* target_addr);

    bool recv(char* buffer, size_t buffer_size, sockaddr_in* client_addr, size_t& received_length,
        std::chrono::microseconds timeout, std::chrono::microseconds poll_interval);
};

#endif