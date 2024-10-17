#include <iostream>
#include <sstream>
#include <thread>

#include <client/net/client.h>
using namespace std;

Client::Client(std::string server_addr_, int server_port_, unsigned int buffer_size_)
    : server_addr(server_addr_), server_port(server_port_), buffer_size(buffer_size_), buffer(new char[buffer_size_])
{}

Client::~Client() { delete[] buffer; }

bool Client::createSocket()
{
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) return false;
    return true;
}

bool Client::connectServer()
{
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(server_port);

    if (inet_pton(AF_INET, server_addr.c_str(), &serv_addr.sin_addr) <= 0) return false;

    if (connect(client_socket, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) return false;

    return true;
}

bool Client::checkBusy()
{
    int valread     = recv(client_socket, buffer, buffer_size, 0);
    buffer[valread] = '\0';
    if (std::string(buffer) != "notfull") return false;
    return true;
}

void Client::enterName()
{
    string       message;
    stringstream ss;
    string       rv;
    while (true)
    {
        cout << "Enter your name: ";
        cin >> message;
        send(client_socket, message.c_str(), message.size(), 0);
        int valread     = recv(client_socket, buffer, buffer_size, 0);
        buffer[valread] = '\0';
        ss              = stringstream(buffer);
        ss >> rv;
        if (rv == "accepted")
        {
            ss >> server_listen_port;
            break;
        }
        else { cout << "Name already taken. Please enter a different name." << endl; }
    }
}

void Client::listenHandler()
{
    int valread = 0;
    while (true)
    {
        valread = recv(client_socket, buffer, buffer_size, 0);
        if (valread <= 0)
        {
            cerr << "Server disconnected or error occurred." << endl;
            CLOSE_SOCKET(client_socket);
            exit(1);
        }
        buffer[valread] = '\0';
        cout << buffer << endl;
    }
}

void Client::sendHandler()
{
    string message;
    while (true)
    {
        getline(cin, message);
        if (message == "/exit")
        {
            CLOSE_SOCKET(client_socket);
            exit(0);
        }
        send(client_socket, message.c_str(), message.length(), 0);
    }
}

void Client::disconnect() { CLOSE_SOCKET(client_socket); }

void Client::run()
{
    if (!createSocket())
    {
        cerr << "Failed to create socket." << endl;
        return;
    }

    if (!connectServer())
    {
        cerr << "Failed to connect to server." << endl;
        return;
    }

    if (!checkBusy())
    {
        cerr << "Server is full." << endl;
        return;
    }

    enterName();
    cout << "Connected to server successfully.\n" << endl;

    thread listener_thread(&Client::listenHandler, this);
    thread sender_thread(&Client::sendHandler, this);

    listener_thread.join();
    sender_thread.join();

    disconnect();
}