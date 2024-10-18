#ifndef __CLIENT_NET_CLIENT_H__
#define __CLIENT_NET_CLIENT_H__

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#include <common/net/socket_defs.h>

class Client
{
  private:
    SOCKET      client_socket;
    std::string server_addr;
    int         server_port;
    int         buffer_size;
    char*       buffer;
    bool        exiting;
    bool        islogin;

    bool        running_;
    std::thread listening_thread_;
    std::mutex  socket_mutex_;

    std::function<void(const std::string&)> on_message_received_;

    bool createSocket();
    bool connectServer();
    bool checkBusy();
    bool enterName(const std::string& username, std::string& error_message);

    void listenHandler();

  public:
    Client(unsigned int buffer_size_);
    ~Client();

    void setTarget(const std::string& server_addr, int server_port);

    bool connectToServer(std::string& error_message);
    bool login(const std::string& username, std::string& error_message);

    void startListening(std::function<void(const std::string&)> on_message_received);
    void sendMessage(const std::string& message);
    void disconnect();
    void stop();
};

#endif