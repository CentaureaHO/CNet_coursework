#include <server/net/session_manager.h>
#include <server/net/session_listener.h>
#include <iostream>

using namespace std;

SessionManager::SessionManager(int base_port, unsigned int listener_threads)
    : server_port(base_port), listener_pool(listener_threads), listener_threads(listener_threads), current_listener(0)
{
    server_socket = bindFreePort(base_port, server_port);
    if (server_socket == INVALID_SOCKET)
    {
        std::cerr << "Failed to bind server socket." << std::endl;
        exit(1);
    }
}

void SessionManager::startListening()
{
    if (listen(server_socket, 5) == SOCKET_ERROR)
    {
        std::cerr << "Failed to start listening on port: " << server_port << std::endl;
        CLOSE_SOCKET(server_socket);
        exit(1);
    }

    std::cout << "Server is listening on port: " << server_port << std::endl;
}

void SessionManager::run()
{
    while (true)
    {
        sockaddr_in client_addr;
        socklen_t   client_len    = sizeof(client_addr);
        SOCKET      client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);

        if (client_socket == INVALID_SOCKET)
        {
            std::cerr << "Failed to accept client connection." << std::endl;
            continue;
        }

        {
            WriteGuard guard = listener_lock.write();
            if (current_listener >= listener_threads)
            {
                cerr << "All listener threads are busy. Rejecting connection from " << inet_ntoa(client_addr.sin_addr)
                     << ":" << ntohs(client_addr.sin_port) << endl;
                reject_connection(client_socket);
                continue;
            }
            ++current_listener;
        }

        accept_connection(client_socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }
}

void SessionManager::accept_connection(SOCKET client_socket, const string& client_ip, int client_port)
{
    WriteGuard guard = listener_lock.write();
    listener_pool.EnQueue([this, client_socket, client_ip, client_port]() {
        SessionListener listener(client_socket, client_port, client_ip, this);
        listener.run();
    });
}

void SessionManager::reject_connection(SOCKET client_socket)
{
    static string rej = "Connection reached maximum capacity. Please try again later.";
    send(client_socket, rej.c_str(), rej.size(), 0);
    CLOSE_SOCKET(client_socket);
}

bool SessionManager::addClient(const std::string& nickname, ClientInfo* client_info)
{
    WriteGuard guard = clients_lock.write();
    if (clients.find(nickname) != clients.end()) { return false; }
    clients[nickname] = client_info;
    return true;
}

bool SessionManager::removeClient(const std::string& nickname)
{
    WriteGuard guard = clients_lock.write();
    if (clients.find(nickname) == clients.end()) return false;
    clients.erase(nickname);
    return true;
}

void SessionManager::subListener()
{
    WriteGuard guard = listener_lock.write();
    --current_listener;
}