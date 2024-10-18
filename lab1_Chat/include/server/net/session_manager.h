#ifndef __SERVER_NET_SESSION_MANAGER_H__
#define __SERVER_NET_SESSION_MANAGER_H__

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <common/thread/pool.h>
#include <common/thread/lock.h>
#include <common/net/socket_defs.h>

class SessionListener;

struct ClientInfo
{
    SOCKET           c_socket;
    int              c_port;
    std::string      c_addr;
    std::string      c_nickname;
    SessionListener* c_session;

    ClientInfo() : c_socket(INVALID_SOCKET), c_port(0), c_addr(""), c_nickname(""), c_session(nullptr) {}
};

struct GroupInfo
{
    std::string              group_name;
    std::vector<ClientInfo*> members;
};

class MessageDispatcher;

class SessionManager
{
  public:
    SOCKET server_socket;

  private:
    friend class SessionListener;

    int server_port;

    ThreadPool   listener_pool;
    unsigned int listener_threads;
    unsigned int current_listener;
    ReWrLock     listener_lock;

    std::unordered_map<std::string, ClientInfo*> clients;
    ReWrLock                                     clients_lock;

    bool running;

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

    ClientInfo& getClient(const std::string& nickname)
    {
        ReadGuard guard = clients_lock.read();
        return *clients[nickname];
    }

    void shutdown();
};

#endif