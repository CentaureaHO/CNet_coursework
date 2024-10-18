#include <client/net/client.h>
#include <bits/stdc++.h>

using namespace std;

int main()
{
    Client client(1024);
    while (true)
    {
        string ip, username;
        int    port;

        cout << "Please enter the server IP address: ";
        cin >> ip;
        cout << "Please enter the server port: ";
        cin >> port;
        cout << "Please enter your username: ";
        cin >> username;

        client.setTarget(ip, port);
        string error_message;
        if (!client.connectToServer(error_message))
        {
            cout << "Failed to connect to server: " << error_message << endl;
            continue;
        }

        if (!client.login(username, error_message))
        {
            cout << "Failed to login: " << error_message << endl;
            continue;
        }

        cout << "Login successful." << endl;

        client.startListening([](const string& message) { cout << message << endl; });
        while (true)
        {
            string message;
            cin >> message;
            if (message == "/disconnect")
            {
                client.stop();
                cout << "Disconnecting..." << endl;
                break;
            }
            client.sendMessage(message);
        }

        int quit = 0;
        cout << "Do you want to quit? (1 for yes, 0 for no): ";
        cin >> quit;
        if (quit == 1) break;
    }
}