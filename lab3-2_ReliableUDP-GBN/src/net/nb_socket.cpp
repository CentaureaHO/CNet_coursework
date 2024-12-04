#include <net/nb_socket.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
using namespace std;

NBSocket::NBSocket(int port, Protocol protocol) : _port(port), _protocol(protocol), _sockfd(INVALID_SOCKET) { init(); }
NBSocket::~NBSocket()
{
    if (_sockfd != INVALID_SOCKET) CLOSE_SOCKET(_sockfd);
}

bool NBSocket::init()
{
    int type = (_protocol == Protocol::UDP) ? SOCK_DGRAM : SOCK_STREAM;
    _sockfd  = socket(AF_INET, type, 0);
    if (_sockfd == INVALID_SOCKET)
    {
        perror("Socket creation failed");
        return false;
    }

    int opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        CLOSE_SOCKET(_sockfd);
        return false;
    }

    sockaddr_in server_addr{};
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(_port);

    if (::bind(_sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        perror("Bind failed");
        CLOSE_SOCKET(_sockfd);
        return false;
    }

    if (_protocol == Protocol::TCP)
    {
        if (listen(_sockfd, SOMAXCONN) == SOCKET_ERROR)
        {
            perror("Listen failed");
            CLOSE_SOCKET(_sockfd);
            return false;
        }
    }

    if (!SetSocketNonBlocking(_sockfd))
    {
        cerr << "Failed to set socket to non-blocking mode.\n";
        CLOSE_SOCKET(_sockfd);
        return false;
    }

    return true;
}

int NBSocket::getBoundPort() const
{
    if (_sockfd == INVALID_SOCKET)
    {
        cerr << "Socket is not initialized.\n";
        return -1;
    }

    sockaddr_in addr{};
    socklen_t   addr_len = sizeof(addr);

    if (getsockname(_sockfd, (struct sockaddr*)&addr, &addr_len) == -1)
    {
        perror("getsockname failed");
        return -1;
    }

    return ntohs(addr.sin_port);
}

bool NBSocket::send(const char* buffer, size_t buffer_size, const sockaddr_in* target_addr)
{
    if (_protocol == Protocol::UDP)
    {
        int sent_len =
            sendto(_sockfd, buffer, buffer_size, 0, (const struct sockaddr*)target_addr, sizeof(sockaddr_in));

        return sent_len == (int)buffer_size;
    }
    else if (_protocol == Protocol::TCP)
    {
        int sent_len = ::send(_sockfd, buffer, buffer_size, 0);
        return sent_len == (int)buffer_size;
    }
    return false;
}

bool NBSocket::recv(char* buffer, size_t buffer_size, sockaddr_in* client_addr, size_t& received_length,
    chrono::microseconds timeout, chrono::microseconds poll_interval)
{
    if (_sockfd == INVALID_SOCKET)
    {
        cerr << "Socket is not initialized.\n";
        return false;
    }

    socklen_t addr_len   = sizeof(sockaddr_in);
    auto      start_time = chrono::high_resolution_clock::now();
    auto      now        = chrono::high_resolution_clock::now();
    auto      elapsed_us = chrono::duration_cast<chrono::microseconds>(now - start_time);

    while (true)
    {
        if (_protocol == Protocol::UDP)
        {
            int recv_len = recvfrom(_sockfd, buffer, buffer_size, 0, (struct sockaddr*)client_addr, &addr_len);

            if (recv_len > 0)
            {
                received_length = static_cast<size_t>(recv_len);
                return true;
            }
        }
        else if (_protocol == Protocol::TCP)
        {
            int recv_len = ::recv(_sockfd, buffer, buffer_size, 0);
            if (recv_len > 0)
            {
                received_length = static_cast<size_t>(recv_len);
                return true;
            }
        }

        now        = chrono::high_resolution_clock::now();
        elapsed_us = chrono::duration_cast<chrono::microseconds>(now - start_time);
        if (elapsed_us >= timeout)
            break;

        this_thread::sleep_for(poll_interval);
    }

    received_length = 0;
    return false;
}