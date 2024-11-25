#include <bits/stdc++.h>
#include <net/socket_defs.h>
#include <net/nb_socket.h>

int main()
{
    NBSocket server(0, Protocol::UDP); // 0 表示自动分配端口

    if (!server.init())
    {
        std::cerr << "Failed to initialize server.\n";
        return 1;
    }

    int actual_port = server.getBoundPort();
    if (actual_port == -1)
    {
        std::cerr << "Failed to retrieve bound port.\n";
        return 1;
    }

    std::cout << "Server is running on port: " << actual_port << "\n";

    char        buffer[2048];
    sockaddr_in client_addr{};
    size_t      received_length = 0;

    while (true)
    {
        bool received = server.recv(buffer,
            sizeof(buffer),
            &client_addr,
            received_length,
            std::chrono::milliseconds(5000),  // 超时时间 5 秒
            std::chrono::milliseconds(100));  // 轮询间隔 100 毫秒

        if (received)
        {
            buffer[received_length] = '\0';
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);

            std::cout << "Received data: " << buffer
                      << " from " << client_ip << ":" << client_port << std::endl;

            // 回发数据
            server.send("Acknowledged", strlen("Acknowledged"), &client_addr);
        }
        else { std::cout << "No data received (timeout).\n"; }
    }

    return 0;
}
