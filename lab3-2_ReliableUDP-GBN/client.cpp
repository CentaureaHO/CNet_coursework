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

    char   buffer[BODY_SIZE];
    size_t totalBytesSent = 0;

    auto start = high_resolution_clock::now();

    while (file)
    {
        file.read(buffer, BODY_SIZE);
        size_t bytesRead = file.gcount();
        if (bytesRead > 0)
        {
            client.send(buffer, bytesRead);
            totalBytesSent += bytesRead;
            // cout << "Sent " << totalBytesSent << " bytes so far..." << endl;
        }
    }

    string endMessage = "File end\r\n";
    client.send(endMessage.c_str(), endMessage.size());

    auto end      = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    if (duration.count() > 0)
    {
        double throughput = (totalBytesSent * 8.0) / duration.count() / 1000;
        cout << "File transfer completed in " << duration.count() << " milliseconds." << endl;
        cout << "Total data sent: " << totalBytesSent << " bytes." << endl;
        cout << "Throughput: " << throughput << " mbps." << endl;
    }
    else { cout << "File transfer completed, but duration is too short to measure throughput." << endl; }

    file.close();
    cout << "File " << fileName << " sent successfully." << endl;
}

int main()
{
    map<int, string> file_map = {
        {1, "resources/1.jpg"},
        {2, "resources/2.jpg"},
        {3, "resources/3.jpg"},
        {4, "resources/helloworld.txt"},
        {5, "resources/small.txt"},
    };

    RUDP client(7777);

    cout << "Client run at port " << client.getBoundPort() << endl;

    client.connect("127.0.0.1", 8000);

    // sendFile(client, "resources/small.txt");
    int i = 0;
    while (true)
    {
        cout << "\n\nChoose file to send(or 0 to exit): ";
        cout << "\n1. 1.jpg\n2. 2.jpg\n3. 3.jpg\n4. helloworld.txt\n5. small.txt\n";

        cin >> i;
        if (i == 0) break;
        if (i < 0 || i > 5)
        {
            cout << "Invalid choice. Please try again." << endl;
            continue;
        }

        sendFile(client, file_map[i]);
    }

    client.disconnect();
}