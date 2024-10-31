#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <string>
#include <socket_defs.h>
#include <atomic>

class HttpServer
{
  public:
    HttpServer(int port);
    bool start();
    void stop();

  private:
    int               port_;
    SOCKET            server_socket_;
    std::atomic<bool> running_;

    void        handleClient(SOCKET client_socket);
    std::string generateResponse(const std::string& content, int status_code = 200,
        const std::string& status_text = "OK", const std::string& content_type = "text/plain");
    std::string readFile(const std::string& file_path);
    std::string getContentType(const std::string& path);
};

#endif