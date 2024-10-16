#include <bits/stdc++.h>

#include <server/net/session_manager.h>
using namespace std;

int main()
{
    SessionManager server(8080, 4);  // 使用8080端口和4个线程的线程池
    server.startListening();
    return 0;
}