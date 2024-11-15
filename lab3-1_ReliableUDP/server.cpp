#include <bits/stdc++.h>
#include <net/packet.h>
#include <net/rudp.h>
#include <thread>
#include <chrono>
using namespace std;

#define ROUTER_PORT 8888
#define LH "127.0.0.1"

int cnt = 3;

void server_recv(UDPConnection& server)
{
    server.listen();
    char buffer[10000];
    while (true)
    {
        uint32_t len = server.recv(buffer, 8192);
        if (len > 0)
        {
            buffer[len] = '\0';
            cout << "Server received: " << buffer << endl;
        }
        else if (len == 0)
            break;
    }
}

int main()
{
    UDPConnection server(LH, 9999, false);

    thread server_thread(server_recv, std::ref(server));
    server_thread.join();

    return 0;
}