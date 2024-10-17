#include <iostream>
#include <sstream>

#include <server/net/session_listener.h>
#include <common/console/format_date.h>
#include <server/net/message_dispatcher.h>
using namespace std;

SessionListener::SessionListener(
    SOCKET client_socket_, int client_port_, string client_addr_, unsigned int buffer_size_, SessionManager* manager_)
    : manager(manager_), buffer(new char[buffer_size_]), buffer_size(buffer_size_)
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
        DERR << "Failed to bind listener socket for client " << client_addr_ << ":" << client_port_ << endl;
        init_success = false;
    }
}

SessionListener::~SessionListener() { delete[] buffer; }

void SessionListener::remove_connection(bool hasRegistered)
{
    if (init_success) CLOSE_SOCKET(listener_socket);
    if (hasRegistered) manager->removeClient(nickname);
    manager->subListener();
}

bool SessionListener::checkName()
{
    static string rej          = "rejected";
    char          buffer[1024] = {0};
    while (true)
    {
        int valread = recv(client_info.c_socket, buffer, 1024, 0);
        if (valread == SOCKET_ERROR)
        {
            DERR << "Failed to receive name from client " << client_info.c_addr << ":" << client_info.c_port << endl;
            return false;
        }
        else if (valread == 0)
        {
            DERR << "Connection closed by client " << client_info.c_addr << ":" << client_info.c_port << endl;
            return false;
        }
        buffer[valread] = '\0';

        nickname = buffer;
        if (nickname != "Server" && manager->addClient(nickname, &client_info))
        {
            stringstream ss;
            ss << "accepted " << assigned_port;
            string acc = ss.str();
            send(client_info.c_socket, acc.c_str(), acc.size(), 0);
            client_info.c_nickname = nickname;
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
        DERR << "Failed to initialize listener for client " << client_info.c_addr << ":" << client_info.c_port << endl;
        return;
    }

    if (!checkName())
    {
        remove_connection(false);
        return;
    }

    for (auto& client : manager->getClients())
    {
        MessageDispatcher::sendToSingle("Server", nickname + " join chat room.", *client.second);
    }

    // cout << "Client " << nickname << " connected, current connection count: " << manager->getClients().size() <<
    // endl;
    DLOG << "Client " << nickname << " connected, current connection count: " << manager->getClients().size() << endl;
    while (true)
    {
        int valread = recv(client_info.c_socket, buffer, buffer_size, 0);
        if (valread == SOCKET_ERROR)
        {
            DERR << "Failed to receive message from client " << nickname << endl;
            break;
        }
        else if (valread == 0)
        {
            DERR << "Connection closed by client " << nickname << endl;
            break;
        }
        buffer[valread] = '\0';

        DLOG << "Received message from " << nickname << ": " << buffer << endl;

        for (auto& client : manager->getClients())
        {
            // if (client.first == nickname) continue;
            MessageDispatcher::sendToSingle(nickname, buffer, *client.second);
        }
    }
    remove_connection(true);
    for (auto& client : manager->getClients())
    {
        MessageDispatcher::sendToSingle("Server", nickname + " exit chat room.", *client.second);
    }
    DLOG << "Client " << nickname << " disconnected, current connection count: " << manager->getClients().size()
         << endl;
}

void SessionListener::closeSession()
{
    shutdown(client_info.c_socket, SHUT_RDWR);
    CLOSE_SOCKET(client_info.c_socket);
}