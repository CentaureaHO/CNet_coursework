#include <iostream>

#include <server/net/session_listener.h>
using namespace std;

SessionListener::SessionListener(SOCKET client_socket_, int client_port_, string client_addr_, SessionManager* manager_)
    : manager(manager_)
{
    client_info.c_socket  = client_socket_;
    client_info.c_port    = client_port_;
    client_info.c_addr    = client_addr_;
    client_info.c_session = this;

    init_success    = true;
    int prev_port   = manager->getPort();
    listener_socket = bindFreePort(prev_port + 1, assigned_port);
    if (listener_socket == INVALID_SOCKET)
    {
        cerr << "Failed to bind listener socket." << std::endl;
        init_success = false;
    }
}

SessionListener::~SessionListener() {}

void SessionListener::remove_connection(bool hasRegistered)
{
    if (init_success) CLOSE_SOCKET(listener_socket);
    if (hasRegistered) manager->removeClient(nickname);
    manager->subListener();
}

bool SessionListener::checkName()
{
    static string acc = "accepted", rej = "rejected";
    char          buffer[1024] = {0};
    while (true)
    {
        int valread = recv(client_info.c_socket, buffer, 1024, 0);
        if (valread == SOCKET_ERROR)
        {
            cerr << "Failed to receive name from client." << endl;
            return false;
        }
        else if (valread == 0)
        {
            cerr << "Connection closed by client." << endl;
            return false;
        }
        buffer[valread] = '\0';

        nickname = buffer;
        if (nickname != "prev" && manager->addClient(nickname, &client_info))
        {
            send(client_info.c_socket, acc.c_str(), acc.size(), 0);
            return true;
        }
        send(client_info.c_socket, rej.c_str(), rej.size(), 0);
    }
    return false;
}

void SessionListener::run()
{
    if (!init_success)
    {
        cerr << "Failed to initialize listener." << endl;
        return;
    }

    if (!checkName())
    {
        remove_connection(false);
        return;
    }

    cout << "Client " << nickname << " connected, current connection count: " << manager->getClients().size() << endl;
    char buffer[1024] = {0};
    recv(client_info.c_socket, buffer, 1024, 0);
    remove_connection(true);
    cout << "Client " << nickname << " disconnected, current connection count: " << manager->getClients().size()
         << endl;
}