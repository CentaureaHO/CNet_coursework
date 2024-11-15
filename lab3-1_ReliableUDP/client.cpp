#include <bits/stdc++.h>
#include <net/packet.h>
#include <net/rudp.h>
#include <thread>
#include <chrono>
using namespace std;

#define LH "127.0.0.1"
#define ROUTER_PORT 8888

int main()
{
    UDPConnection client(LH, 8080);

    this_thread::sleep_for(chrono::seconds(1));
    client.connect(LH, ROUTER_PORT);

    char client_data[15] = "Hello, server!";
    client.send(client_data, strlen(client_data));
    client_data[0] = '1';
    client.send(client_data, strlen(client_data));
    client_data[0] = '2';
    client.send(client_data, strlen(client_data));

    client.disconnect();

    return 0;
}