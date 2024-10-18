#include <server/net/session_manager.h>
#include <server/net/session_listener.h>
#include <server/net/message_dispatcher.h>
#include <common/console/format_date.h>
#include <iostream>
#include <thread>

using namespace std;

SessionManager::SessionManager(int base_port, unsigned int listener_threads)
    : server_port(base_port), listener_pool(listener_threads), listener_threads(listener_threads), current_listener(0)
{
    server_socket = bindFreePort(base_port, server_port);
    if (server_socket == INVALID_SOCKET)
    {
        DERR << "Failed to bind server socket." << endl;
        exit(1);
    }
}

void SessionManager::startListening()
{
    if (listen(server_socket, 5) == SOCKET_ERROR)
    {
        DERR << "Failed to start listening on port: " << server_port << endl;
        CLOSE_SOCKET(server_socket);
        exit(1);
    }

    DLOG << "Server is listening on port: " << server_port << endl;
}

void SessionManager::run()
{
    /*
    if (!SetSocketNonBlocking(server_socket))
    {
        DERR << "Failed to set server socket to non-blocking mode." << endl;
        return;
    }
    */
    running = true;
    thread messageSender(MessageDispatcher::dispatchMessages);
    messageSender.detach();
    while (running)
    {
        sockaddr_in client_addr;
        socklen_t   client_len = sizeof(client_addr);
        // cout << "Waiting for client connection..." << endl;
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        // cout << "Client connected." << endl;

        if (!running) break;

        if (client_socket == INVALID_SOCKET)
        {
#ifdef _WIN32
            int error_code = WSAGetLastError();
            if (error_code == WSAEWOULDBLOCK)
            {
                this_thread::sleep_for(chrono::milliseconds(100));
                continue;
            }
            else if (error_code == WSAECONNABORTED)
            {
                DLOG << "Server socket has been closed, exiting run loop." << endl;
                break;
            }
#else
            if (errno == EWOULDBLOCK)
            {
                this_thread::sleep_for(chrono::milliseconds(100));
                continue;
            }
            else if (errno == EBADF || errno == EINTR)
            {
                DLOG << "Server socket has been closed, exiting run loop." << endl;
                break;
            }
#endif
            else
            {
                DERR << "Failed to accept client connection from " << inet_ntoa(client_addr.sin_addr) << ":"
                     << ntohs(client_addr.sin_port) << endl;
                continue;
            }
        }

        {
            WriteGuard guard = listener_lock.write();
            if (current_listener >= listener_threads)
            {
                DERR << "All listener threads are busy. Rejecting connection from " << inet_ntoa(client_addr.sin_addr)
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
    static string acc = "notfull";
    send(client_socket, acc.c_str(), acc.size(), 0);
    WriteGuard guard = listener_lock.write();
    listener_pool.EnQueue([this, client_socket, client_ip, client_port]() {
        SessionListener listener(client_socket, client_port, client_ip, 2048, this);
        listener.run();
    });
}

void SessionManager::reject_connection(SOCKET client_socket)
{
    static string rej = "full";
    send(client_socket, rej.c_str(), rej.size(), 0);
    CLOSE_SOCKET(client_socket);
}

bool SessionManager::addClient(const string& nickname, ClientInfo* client_info)
{
    WriteGuard guard = clients_lock.write();
    if (clients.find(nickname) != clients.end()) { return false; }
    clients[nickname] = client_info;
    return true;
}

bool SessionManager::removeClient(const string& nickname)
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

void SessionManager::shutdown()
{
    WriteGuard cltg = clients_lock.write();
    WriteGuard lltg = listener_lock.write();
    MessageDispatcher::clearQueue();

    DLOG << "Shutting down server..." << endl;
    running = false;

    listener_pool.StopPool();
    CLOSE_SOCKET(server_socket);

    for (auto& client_pair : clients)
    {
        ClientInfo* client_info = client_pair.second;
        if (client_info->c_session) { client_info->c_session->closeSession(); }
    }
    clients.clear();

    DLOG << "All client sessions have been closed." << endl;
    _Exit(0);
}