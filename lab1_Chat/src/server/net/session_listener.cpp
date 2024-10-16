#include <iostream>
#include <sstream>

#include <server/net/session_listener.h>
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
        cerr << "Failed to bind listener socket." << std::endl;
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
        if (manager->addClient(nickname, &client_info))
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
        cerr << "Failed to initialize listener." << endl;
        return;
    }

    if (!checkName())
    {
        remove_connection(false);
        return;
    }

    cout << "Client " << nickname << " connected, current connection count: " << manager->getClients().size() << endl;
    while (true)
    {
        int valread = recv(client_info.c_socket, buffer, buffer_size, 0);
        if (valread == SOCKET_ERROR)
        {
            cerr << "Failed to receive message from client " << nickname << endl;
            break;
        }
        else if (valread == 0)
        {
            cerr << "Connection closed by client " << nickname << endl;
            break;
        }
        buffer[valread] = '\0';

        cout << "Received message from " << nickname << ": " << buffer << endl;
        /*
        for (auto& client : manager->getClients())
        {
            if (client.first != nickname) { send(client.second->c_socket, buffer, valread, 0); }
        }
        */
        ClientInfo& client = manager->getClient(nickname);
        MessageDispatcher::sendToSingle(nickname, buffer, client);
    }
    remove_connection(true);
    cout << "Client " << nickname << " disconnected, current connection count: " << manager->getClients().size()
         << endl;
}