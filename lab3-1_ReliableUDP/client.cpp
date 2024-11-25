#include <iostream>
#include <net/socket_defs.h>
#include <net/nb_socket.h>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = std::stoi(argv[1]); // 从命令行参数获取端口号

    SocketInitializer::getInstance();

    NBSocket client(0, Protocol::UDP); // 客户端使用随机端口

    if (!client.init())
    {
        std::cerr << "Failed to initialize client.\n";
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    const char* message = "Hello from client!";
    if (client.send(message, strlen(message), &server_addr))
    {
        std::cout << "Message sent to server on port " << port << ".\n";

        // 等待服务器响应
        char        buffer[2048];
        sockaddr_in response_addr{};
        size_t      received_length = 0;

        bool received = client.recv(buffer,
            sizeof(buffer),
            &response_addr,
            received_length,
            std::chrono::milliseconds(5000),  // 超时时间 5 秒
            std::chrono::milliseconds(100));  // 轮询间隔 100 毫秒

        if (received)
        {
            buffer[received_length] = '\0';
            std::cout << "Response from server: " << buffer << std::endl;
        }
        else
        {
            std::cout << "No response from server (timeout).\n";
        }
    }
    else
    {
        std::cerr << "Failed to send message.\n";
    }

    return 0;
}
