
#include <sstream>
#include <iostream>
#include <http_message.h>
using namespace std;

bool HttpMessage::parseRequest(const string& raw_request)
{
    istringstream stream(raw_request);
    string        method_str, path, version;
    if (!(stream >> method_str >> path >> version)) return false;

    if (method_str == "GET")
        method_ = Method::GET;
    else if (method_str == "POST")
        method_ = Method::POST;
    else
        method_ = Method::UNSUPPORTED;

    path_    = path;
    version_ = version;

    string line;
    while (getline(stream, line) && line != "\r")
    {
        auto colon_pos = line.find(':');
        if (colon_pos != string::npos)
        {
            string key    = line.substr(0, colon_pos);
            string value  = line.substr(colon_pos + 1);
            headers_[key] = value;
        }
    }

    if (stream) getline(stream, body_, '\0');

    return true;
}

void HttpMessage::setHeader(const string& key, const string& value) { headers_[key] = value; }

string HttpMessage::getHeader(const string& key) const
{
    auto it = headers_.find(key);
    return it != headers_.end() ? it->second : "";
}

void HttpMessage::setBody(const string& body)
{
    body_ = body;
    setHeader("Content-Length", to_string(body.size()));
}

string HttpMessage::getBody() const { return body_; }

void HttpMessage::setStatusCode(int code) { status_code_ = code; }

int HttpMessage::getStatusCode() const { return status_code_; }

string HttpMessage::buildResponse() const
{
    ostringstream response;

    response << "HTTP/1.1 " << status_code_ << " OK\r\n";

    for (const auto& header : headers_) response << header.first << ": " << header.second << "\r\n";
    response << "\r\n";

    response << body_;
    return response.str();
}

void HttpMessage::setMethod(Method method) { method_ = method; }

void HttpMessage::setPath(const string& path) { path_ = path; }

void HttpMessage::setVersion(const string& version) { version_ = version; }

HttpMessage::Method HttpMessage::getMethod() const { return method_; }

string HttpMessage::getPath() const { return path_; }

string HttpMessage::getVersion() const { return version_; }