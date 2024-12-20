#ifndef __SOCKET_DEFS_H__
#define __SOCKET_DEFS_H__

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#define CLOSE_SOCKET(s) closesocket(s)
#define SOCKCLEANUP() WSACleanup()
#else
#include <arpa/inet.h>
#include <fcntl.h>  // For fcntl() to set non-blocking on Linux/Unix
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKET int
#define CLOSE_SOCKET(s) close(s)
#define SOCKCLEANUP()
#endif

class SocketInitializer
{
  private:
    SocketInitializer();
    ~SocketInitializer();

  public:
    static SocketInitializer& getInstance();
};

SOCKET bindFreePort(int start_port, int& assigned_port);
bool   SetSocketNonBlocking(SOCKET sock);

#endif