#ifndef __SERVER_NET_SESSION_LISTENER_H__
#define __SERVER_NET_SESSION_LISTENER_H__

#include <string>

#include <common/net/socket_defs.h>
#include <server/net/session_manager.h>

class SessionListener
{
  private:
    SessionManager* manager;
    SOCKET          listener_socket;
    int             assigned_port;
    bool            init_success;
    ClientInfo      client_info;
    std::string     nickname;
    char*           buffer;
    unsigned int    buffer_size;

    void remove_connection(bool hasRegistered = true);
    bool checkName();

  public:
    SessionListener(SOCKET client_socket_, int client_port_, std::string client_addr_, unsigned int buffer_size_,
        SessionManager* manager_ = nullptr);
    ~SessionListener();

    void run();
    void closeSession();
};

#endif