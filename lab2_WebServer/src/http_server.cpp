#include <iostream>
#include <fstream>
#include <sstream>
#include <http_server.h>
#include <http_message.h>
using namespace std;

HttpServer::HttpServer(int port) : port_(port), server_socket_(INVALID_SOCKET) {}

bool HttpServer::start()
{

    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == INVALID_SOCKET)
    {
        cerr << "Failed to create socket\n";
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port_);

    if (::bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        cerr << "Bind failed\n";
        CLOSE_SOCKET(server_socket_);
        return false;
    }

    if (listen(server_socket_, 5) == SOCKET_ERROR)
    {
        cerr << "Listen failed\n";
        CLOSE_SOCKET(server_socket_);
        return false;
    }

    cout << "HTTP Server is running on port " << port_ << endl;

    while (true)
    {
        SOCKET client_socket = accept(server_socket_, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET)
        {
            cerr << "Accept failed\n";
            continue;
        }
        handleClient(client_socket);
        CLOSE_SOCKET(client_socket);
    }

    CLOSE_SOCKET(server_socket_);
    return true;
}

void HttpServer::handleClient(SOCKET client_socket)
{
    const int buffer_size = 4096;
    char      buffer[buffer_size];

    int bytes_received = recv(client_socket, buffer, buffer_size - 1, 0);
    if (bytes_received <= 0)
    {
        cerr << "Receive failed\n";
        return;
    }

    buffer[bytes_received] = '\0';
    string request(buffer);

    HttpMessage http_request;
    if (!http_request.parseRequest(request))
    {
        cerr << "Failed to parse request\n";
        return;
    }

    HttpMessage http_response;
    if (http_request.getMethod() == HttpMessage::Method::GET)
    {
        string path = http_request.getPath();
        if (path == "/") path = "/index.html";

        string file_content = readFile("dist" + path);
        if (!file_content.empty())
        {
            http_response.setStatusCode(200);
            http_response.setHeader("Content-Type", getContentType(path));
            http_response.setBody(file_content);
        }
        else
        {
            http_response.setStatusCode(404);
            http_response.setBody("404 Not Found");
        }
    }
    else if (http_request.getMethod() == HttpMessage::Method::POST)
    {
        string body = http_request.getBody();

        size_t pos = body.find_last_of("\r\n\r\n");
        string post_data;

        if (pos != string::npos)
            post_data = body.substr(pos + 1);
        else
            post_data = body;

        http_response.setStatusCode(200);
        http_response.setBody("POST request received with data: " + post_data);
    }
    else
    {
        http_response.setStatusCode(405);
        http_response.setBody("Method Not Allowed");
    }

    string response = http_response.buildResponse();
    send(client_socket, response.c_str(), response.length(), 0);
}

string HttpServer::generateResponse(
    const string& content, int status_code, const string& status_text, const string& content_type)
{
    ostringstream response_stream;
    response_stream << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    response_stream << "Content-Length: " << content.size() << "\r\n";
    response_stream << "Content-Type: " << content_type << "\r\n";
    response_stream << "Connection: close\r\n";
    response_stream << "\r\n";
    response_stream << content;
    return response_stream.str();
}

string HttpServer::readFile(const string& file_path)
{
    ifstream file(file_path, ios::binary);
    if (!file.is_open()) return "";

    ostringstream content;
    content << file.rdbuf();
    return content.str();
}

string HttpServer::getContentType(const string& path)
{
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css")) return "text/css";
    if (path.ends_with(".js")) return "application/javascript";
    if (path.ends_with(".png")) return "image/png";
    if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
    if (path.ends_with(".gif")) return "image/gif";
    return "application/octet-stream";
}