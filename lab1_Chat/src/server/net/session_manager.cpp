#include <server/net/session_manager.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * (x))  // Sleep 接受毫秒数，因此 x 乘以 1000
#else
#include <unistd.h>
#endif

SessionManager::SessionManager(int base_port, unsigned int listener_threads)
    : server_port(base_port), listener_pool(listener_threads)
{
    server_socket = bindFreePort(base_port, server_port);
    if (server_socket == INVALID_SOCKET)
    {
        std::cerr << "Failed to bind server socket." << std::endl;
        exit(1);
    }
}

void SessionManager::startListening()
{
    if (listen(server_socket, 5) == SOCKET_ERROR)
    {
        std::cerr << "Failed to start listening on port: " << server_port << std::endl;
        CLOSE_SOCKET(server_socket);
        exit(1);
    }

    std::cout << "Server is listening on port: " << server_port << std::endl;

    while (true)
    {
        sockaddr_in client_addr;
        socklen_t   client_len    = sizeof(client_addr);
        SOCKET      client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);

        if (client_socket == INVALID_SOCKET)
        {
            std::cerr << "Failed to accept client connection." << std::endl;
            continue;
        }

        std::string client_ip   = inet_ntoa(client_addr.sin_addr);
        int         client_port = ntohs(client_addr.sin_port);

        std::cout << "Accepted connection from " << client_ip << ":" << client_port << std::endl;

        ClientInfo client_info;
        client_info.c_socket = client_socket;
        client_info.c_port   = client_port;
        client_info.c_addr   = client_ip;

        {
            WriteGuard guard   = clients_lock.write();
            clients[client_ip] = client_info;
        }

        listener_pool.EnQueue([client_socket, client_ip, client_port] {
            std::cout << "Handling client " << client_ip << ":" << client_port << std::endl;
            sleep(5);
            std::cout << "Done handling client " << client_ip << ":" << client_port << std::endl;
            CLOSE_SOCKET(client_socket);
        });
    }
}