#include <bits/stdc++.h>
#include <net/socket_defs.h>
#include <net/nb_socket.h>
#include <net/rudp/rudp_defs.h>
#include <net/rudp/rudp.h>
using namespace std;
using namespace chrono;

void sendFile(RUDP& client, const string& filePath)
{
    ifstream file(filePath, ios::binary);
    if (!file.is_open())
    {
        cerr << "Failed to open file: " << filePath << endl;
        return;
    }

    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

    string beginMessage = "File begin: " + fileName + "\r\n";
    client.send(beginMessage.c_str(), beginMessage.size());

    char buffer[BODY_SIZE];
    while (file)
    {
        file.read(buffer, BODY_SIZE);
        size_t bytesRead = file.gcount();
        if (bytesRead > 0) { client.send(buffer, bytesRead); }
    }

    string endMessage = "File end\r\n";
    client.send(endMessage.c_str(), endMessage.size());

    file.close();
    cout << "File " << fileName << " sent successfully." << endl;
}

int main()
{
    RUDP client(7777);

    cout << "Client run at port " << client.getBoundPort() << endl;

    client.connect("127.0.0.1", 8000);

    auto start = high_resolution_clock::now();

    // sendFile(client, "resources/1.jpg");

    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    cout << "File transfer completed in " << duration.count() << " milliseconds." << endl;

    client.disconnect();
}