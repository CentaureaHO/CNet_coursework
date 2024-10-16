#include <iostream>

#include <common/net/socket_defs.h>

SocketInitializer::SocketInitializer()
{
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) std::cerr << "WSAStartup failed." << std::endl;
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