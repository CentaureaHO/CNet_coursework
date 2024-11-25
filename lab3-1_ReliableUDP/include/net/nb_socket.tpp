#include <iostream>
#include <chrono>
#include <thread>

template <typename Duration>
bool NBSocket::recv(char* buffer, size_t buffer_size, sockaddr_in* client_addr, size_t& received_length,
    Duration timeout, Duration poll_interval)
{
    if (_sockfd == INVALID_SOCKET)
    {
        std::cerr << "Socket is not initialized.\n";
        return false;
    }

    socklen_t addr_len   = sizeof(sockaddr_in);
    auto      start_time = std::chrono::high_resolution_clock::now();

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
#ifdef _WIN32
            else if (recv_len == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
                perror("recvfrom failed");
#else
            else if (recv_len < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
                perror("recvfrom failed");
#endif
        }
        else if (_protocol == Protocol::TCP)
        {
            int recv_len = ::recv(_sockfd, buffer, buffer_size, 0);
            if (recv_len > 0)
            {
                received_length = static_cast<size_t>(recv_len);
                return true;
            }
#ifdef _WIN32
            else if (recv_len == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
                perror("recv failed");
#else
            else if (recv_len < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
                perror("recv failed");
#endif
        }

        auto now = std::chrono::high_resolution_clock::now();
        if (now - start_time >= timeout) { break; }

        std::this_thread::sleep_for(poll_interval);
    }

    received_length = 0;
    return false;
}