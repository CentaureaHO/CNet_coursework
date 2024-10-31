#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <fstream>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#include <boost/beast/core.hpp>
#pragma GCC diagnostic pop
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
using namespace std;

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = boost::asio::ip::tcp;

class HttpSession : public enable_shared_from_this<HttpSession>
{
  public:
    explicit HttpSession(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { readRequest(); }

  private:
    tcp::socket                       socket_;
    beast::flat_buffer                buffer_;
    http::request<http::string_body>  req_;
    http::response<http::string_body> res_;

    void readRequest()
    {
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, req_, [self](beast::error_code ec, size_t) {
            if (!ec) self->processRequest();
        });
    }

    void processRequest()
    {
        if (req_.method() == http::verb::get) { handleGetRequest(); }
        else if (req_.method() == http::verb::post) { handlePostRequest(); }
        else
        {
            res_ = {http::status::method_not_allowed, req_.version()};
            res_.set(http::field::content_type, "text/plain");
            res_.body() = "Unsupported HTTP method";
            res_.prepare_payload();
            writeResponse();
        }
    }

    void handleGetRequest()
    {
        string path(req_.target());

        if (path == "/") path = "/index.html";
        path = "dist" + path;

        ifstream file(path, ios::binary);
        if (!file)
        {
            res_ = {http::status::not_found, req_.version()};
            res_.set(http::field::content_type, "text/plain");
            res_.body() = "The resource was not found.";
        }
        else
        {
            res_ = {http::status::ok, req_.version()};
            res_.set(http::field::content_type, getContentType(path));
            res_.body() = string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        }
        res_.set(http::field::server, "Boost.Beast");
        res_.prepare_payload();
        writeResponse();
    }

    void handlePostRequest()
    {
        res_ = {http::status::ok, req_.version()};
        res_.set(http::field::content_type, "text/plain");
        res_.body() = "POST request received with body: " + req_.body();
        res_.prepare_payload();
        writeResponse();
    }

    void writeResponse()
    {
        auto self = shared_from_this();
        http::async_write(socket_, res_, [self](beast::error_code ec, size_t) {
            self->socket_.shutdown(tcp::socket::shutdown_send, ec);
        });
    }

    string getContentType(const string& path)
    {
        if (path.ends_with(".html")) return "text/html";
        if (path.ends_with(".css")) return "text/css";
        if (path.ends_with(".js")) return "application/javascript";
        if (path.ends_with(".png")) return "image/png";
        if (path.ends_with(".jpg") || path.ends_with(".jpeg")) return "image/jpeg";
        return "application/octet-stream";
    }
};

class HttpServer
{
  public:
    HttpServer(net::io_context& ioc, tcp::endpoint endpoint) : acceptor_(ioc, endpoint) { doAccept(); }

  private:
    tcp::acceptor acceptor_;

    void doAccept()
    {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) make_shared<HttpSession>(std::move(socket))->start();
            doAccept();
        });
    }
};

int main()
{
    string               address_ = "127.0.0.1";
    const auto           address  = net::ip::make_address(address_);
    const unsigned short port     = 8080;

    net::io_context ioc{1};
    tcp::endpoint   endpoint{address, port};
    HttpServer      server(ioc, endpoint);

    cout << "HTTP Server is running on " << address_ << endl;
    ioc.run();
}