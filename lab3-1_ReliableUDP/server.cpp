#include <bits/stdc++.h>
#include <net/socket_defs.h>
#include <net/nb_socket.h>
#include <net/rudp/rudp_defs.h>
#include <net/rudp/rudp.h>
using namespace std;

void receiveFile(RUDP_P& packet, ofstream& outFile, bool& receivingFile)
{
    string data(packet.body, packet.header.data_len);

    if (data.find("File begin: ") == 0)
    {
        // Start receiving a new file
        string fileName = data.substr(12, data.find("\r\n") - 12);
        string filePath = "download/" + fileName;

        outFile.open(filePath, ios::binary);
        if (!outFile.is_open())
        {
            cerr << "Failed to open file: " << filePath << endl;
            receivingFile = false;
            return;
        }

        cout << "Receiving file: " << fileName << endl;
        receivingFile = true;
    }
    else if (data == "File end\r\n")
    {
        // End of file transmission
        if (outFile.is_open())
        {
            outFile.close();
            cout << "File received successfully." << endl;
        }
        receivingFile = false;
    }
    else if (receivingFile)
    {
        // Write file data
        if (outFile.is_open()) { outFile.write(packet.body, packet.header.data_len); }
        else { cerr << "File output stream is not open." << endl; }
    }
}

int main()
{
    RUDP server(8888);

    cout << "Server run at port " << server.getBoundPort() << endl;

    ofstream outFile;
    bool     receivingFile = false;

    server.listen([&](RUDP_P& packet) { receiveFile(packet, outFile, receivingFile); });
}