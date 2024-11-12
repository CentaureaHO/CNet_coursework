#include <iostream>
#include <net/socket_defs.h>
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