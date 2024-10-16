#include <bits/stdc++.h>

#include <server/net/session_manager.h>
using namespace std;

int main()
{
    SessionManager server(8080, 4);
    server.startListening();
    server.run();
    return 0;
}