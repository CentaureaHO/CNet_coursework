#include <bits/stdc++.h>

#include <client/net/client.h>
using namespace std;

int main()
{
    string addr;
    int    port;

    cout << "Enter server address: ";
    cin >> addr;
    cout << "Enter server port: ";
    cin >> port;

    Client client(addr, port, 1024);
    client.run();
    return 0;
}