#include <iostream>
#include <sstream>
#include <thread>
#include <cstring>

#include <client/net/client.h>
using namespace std;

Client::Client(unsigned int buffer_size_)
    : client_socket(INVALID_SOCKET),
      server_addr("127.0.0.1"),
      server_port(8080),
      buffer_size(buffer_size_),
      buffer(new char[buffer_size_]),
      exiting(false),
      islogin(false),
      running_(false)
{}

Client::~Client()
{
    stop();
    if (buffer) delete[] buffer;
}

void Client::setTarget(const std::string& server_addr_, int server_port_)
{
    if (islogin) return;
    server_addr = server_addr_;
    server_port = server_port_;
}

bool Client::createSocket()
{
    if (islogin) return false;
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) return false;
    return true;
}

bool Client::connectServer()
{
    if (islogin) return false;
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(server_port);

    if (inet_pton(AF_INET, server_addr.c_str(), &serv_addr.sin_addr) <= 0) return false;
    if (connect(client_socket, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) return false;

    return true;
}

bool Client::checkBusy()
{
    if (islogin) return false;
    int valread = recv(client_socket, buffer, buffer_size, 0);
    if (valread <= 0) return false;
    buffer[valread] = '\0';
    if (std::string(buffer) != "notfull") return false;
    return true;
}

bool Client::enterName(const std::string& username, std::string& error_message)
{
    if (islogin) return false;
    string       message = username;
    stringstream ss;
    string       rv;

    send(client_socket, message.c_str(), message.size(), 0);
    int valread = recv(client_socket, buffer, buffer_size, 0);
    if (valread <= 0)
    {
        error_message = "无法接收服务器响应。";
        return false;
    }
    buffer[valread] = '\0';
    ss              = stringstream(buffer);
    ss >> rv;
    if (rv == "accepted")
    {
        // ss >> server_listen_port;
        return true;
    }
    else
    {
        error_message = "用户名已被占用，请更换。";
        return false;
    }
}

void Client::listenHandler()
{
    int valread = 0;
    while (running_)
    {
        valread = recv(client_socket, buffer, buffer_size, 0);
        if (valread <= 0)
        {
            if (exiting) return;
            running_ = false;
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                CLOSE_SOCKET(client_socket);
            }
            if (on_message_received_) { on_message_received_("服务器断开连接或发生错误。"); }
            return;
        }
        else
        {
            buffer[valread] = '\0';
            std::string message(buffer);

            static std::string exit_message = "/disconnect";
            if (message == exit_message)
            {
                running_ = false;
                if (on_message_received_) { on_message_received_("收到断开信号，正在关闭连接..."); }
                return;
            }

            if (on_message_received_) { on_message_received_(message); }
        }
    }
}

void Client::disconnect()
{
    CLOSE_SOCKET(client_socket);
    islogin = false;
}

bool Client::connectToServer(std::string& error_message)
{
    if (!createSocket())
    {
        error_message = "无法创建套接字。";
        return false;
    }

    if (!connectServer())
    {
        error_message = "无法连接到服务器，请检查 IP 地址和端口号。";
        return false;
    }

    if (!checkBusy())
    {
        error_message = "服务器人数达到上限，请稍后再试。";
        return false;
    }

    return true;
}

bool Client::login(const std::string& username, std::string& error_message)
{
    return enterName(username, error_message);
}

void Client::startListening(std::function<void(const std::string&)> on_message_received)
{
    on_message_received_ = on_message_received;
    running_             = true;
    listening_thread_    = std::thread(&Client::listenHandler, this);
}

void Client::sendMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (running_ && client_socket != INVALID_SOCKET) send(client_socket, message.c_str(), message.length(), 0);
}

void Client::stop()
{
    if (!running_) return;
    exiting  = true;
    running_ = false;
    islogin  = false;
    if (client_socket != INVALID_SOCKET)
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        static std::string          disconnect_message = "/disconnect";
        send(client_socket, disconnect_message.c_str(), disconnect_message.length(), 0);
        CLOSE_SOCKET(client_socket);
        client_socket = INVALID_SOCKET;
    }
    if (listening_thread_.joinable()) { listening_thread_.join(); }
}

void Client::errHandler()
{
    if (!running_) return;
    exiting  = true;
    running_ = false;
    islogin  = false;
    if (client_socket != INVALID_SOCKET)
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        CLOSE_SOCKET(client_socket);
        client_socket = INVALID_SOCKET;
    }
    if (listening_thread_.joinable()) { listening_thread_.join(); }
}