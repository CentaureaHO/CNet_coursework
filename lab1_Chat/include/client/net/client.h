#ifndef __CLIENT_NET_CLIENT_H__
#define __CLIENT_NET_CLIENT_H__

#include <string>

#include <common/net/socket_defs.h>

class Client
{
  private:
    SOCKET      client_socket;
    std::string server_addr;
    int         server_port;
    int         server_listen_port;
    int         buffer_size;
    char*       buffer;
    bool        exiting;

    bool createSocket();
    bool connectServer();
    bool checkBusy();
    void enterName();

    void listenHandler();
    void sendHandler();

    void disconnect();

  public:
    Client(std::string server_addr_, int server_port_, unsigned int buffer_size_);
    ~Client();

    void run();
};

#endif