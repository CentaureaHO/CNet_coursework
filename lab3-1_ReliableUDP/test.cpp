#include <bits/stdc++.h>
#include <net/packet.h>
#include <net/rudp.h>
#include <thread>
#include <chrono>
using namespace std;

#define ROUTER_PORT 8888
#define LH "127.0.0.1"

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
            // break;
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

int main()
{
    UDPConnection client(LH, 8080);
    UDPConnection server(LH, 9999);

    thread server_thread(server_recv, std::ref(server));
    this_thread::sleep_for(chrono::seconds(1));
    client.connect(LH, 9999);

    char client_data[8192] = "Hello, server!";
    client.send(client_data, strlen(client_data));
    client.send(client_data, strlen(client_data));
    client.send(client_data, strlen(client_data));

    this_thread::sleep_for(chrono::seconds(1));

    server_thread.detach();

    return 0;
}