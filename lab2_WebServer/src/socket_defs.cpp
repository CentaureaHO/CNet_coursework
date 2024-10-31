#include <iostream>

#include <socket_defs.h>

using namespace std;

SocketInitializer::SocketInitializer()
{
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        cerr << "WSAStartup failed." << endl;
        exit(1);
    }
#endif
}

SocketInitializer::~SocketInitializer() { SOCKCLEANUP(); }

SocketInitializer& SocketInitializer::getInstance()
{
    static SocketInitializer instance;
    return instance;
}

namespace
{
    SocketInitializer& socket_initializer = SocketInitializer::getInstance();
}

SOCKET bindFreePort(int start_port, int& assigned_port)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        cerr << "Failed to create socket." << endl;
        return INVALID_SOCKET;
    }

    sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    for (int port = start_port; port <= 65535; ++port)
    {
        addr.sin_port = htons(port);

        if (::bind(sock, (sockaddr*)&addr, sizeof(addr)) == 0)
        {
            assigned_port = port;
            return sock;
        }
    }

    CLOSE_SOCKET(sock);
    cerr << "No available port found." << endl;
    return INVALID_SOCKET;
}

bool SetSocketNonBlocking(SOCKET sock)
{
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) { return false; }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) { return false; }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) { return false; }
#endif
    return true;
}