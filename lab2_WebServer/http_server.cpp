#include <http_server.h>
#include <iostream>

int main()
{
    HttpServer server(8080);
    if (!server.start())
    {
        std::cerr << "Failed to start the server.\n";
        return -1;
    }
    return 0;
}