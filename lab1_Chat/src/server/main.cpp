#include <bits/stdc++.h>
#include <server/net/session_manager.h>
using namespace std;

int main()
{
    SessionManager server(8080, 4);
    server.startListening();

    thread controlThread([&server]() {
        string command;
        while (true)
        {
            // cout << "> ";
            getline(cin, command);
            if (command == "shutdown")
            {
                server.shutdown();
                break;
            }
        }
    });

    server.run();

    controlThread.join();

    return 0;
}