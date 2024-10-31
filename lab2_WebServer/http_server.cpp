#include <http_server.h>
#include <iostream>
#include <thread>
using namespace std;

int main()
{
    HttpServer server(8080);

    thread server_thread([&server]() {
        if (!server.start()) { cerr << "Failed to start the server.\n"; }
    });

    cout << "Enter 'q' to stop the server." << endl;
    string command;
    while (true)
    {
        cin >> command;
        if (command == "q")
        {
            server.stop();
            break;
        }
    }

    server_thread.join();
    cout << "Server stopped." << endl;
}