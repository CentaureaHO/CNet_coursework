#ifndef __SERVER_NET_SESSION_MANAGER_H__
#define __SERVER_NET_SESSION_MANAGER_H__

#include <mutex>
#include <string>
#include <unordered_map>

#include <common/thread/pool.h>
#include <common/thread/lock.h>
#include <common/net/socket_defs.h>

class SessionListener;

struct ClientInfo
{
    SOCKET           c_socket;
    int              c_port;
    std::string      c_addr;
    SessionListener* c_session;

    ClientInfo() : c_socket(INVALID_SOCKET), c_port(0), c_addr(""), c_session(nullptr) {}
};

class SessionManager
{
  private:
    SOCKET server_socket;
    int    server_port;

    ThreadPool listener_pool;

    std::unordered_map<std::string, ClientInfo> clients;
    ReWrLock                                    clients_lock;

  public:
    SessionManager(int base_port = 8080, unsigned int listener_threads = 4);

    void startListening();
};

#endif