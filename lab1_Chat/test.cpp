#include <iostream>
#include <cstring>
#include <sstream>
#include "include/common/net/socket_defs.h"

int main()
{
    SocketInitializer::getInstance();

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Connection Failed" << std::endl;
        return -1;
    }

    std::cout << "Connected to server successfully" << std::endl;

    char buffer[1024] = {0};
    {
        int valread = recv(sock, buffer, 1024, 0);
        buffer[valread] = '\0';
        if (std::string(buffer) != "notfull")
        {
            std::cerr << "Failed to connect to server" << std::endl;
            return -1;
        }
    }
    std::string message = "prev";
    std::stringstream ss;
    std::string rv;
    int port = 0;
    while(true)
    {
        std::cin >> message;
        send(sock, message.c_str(), message.size(), 0);
        int valread = recv(sock, buffer, 1024, 0);
        buffer[valread] = '\0';
        std::cout << buffer << std::endl;
        ss = std::stringstream(buffer);
        ss >> rv;
        if (rv == "accepted")
        {
            ss >> port;
            break;
        }
    }
    std::cout << "Accepted on port: " << port << std::endl;

    int a = 0;
    std::cin >> a;

    CLOSE_SOCKET(sock);

    return 0;
}