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

    ThreadPool   listener_pool;
    unsigned int listener_threads;
    unsigned int current_listener;
    ReWrLock     listener_lock;

    std::unordered_map<std::string, ClientInfo*> clients;
    ReWrLock                                     clients_lock;

  public:
    SessionManager(int base_port = 8080, unsigned int listener_threads = 4);

    void startListening();
    void run();

    void accept_connection(SOCKET client_socket, const std::string& client_ip, int client_port);
    void reject_connection(SOCKET client_socket);

    int                                           getPort() const { return server_port; }
    std::unordered_map<std::string, ClientInfo*>& getClients() { return clients; }

    bool addClient(const std::string& nickname, ClientInfo* client_info);
    bool removeClient(const std::string& nickname);
    void subListener();
};

#endif