#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#include <string>
#include <unordered_map>

class HttpMessage
{
  public:
    enum class Method
    {
        GET,
        POST,
        UNSUPPORTED
    };

    HttpMessage() = default;

    bool parseRequest(const std::string& raw_request);

    void        setHeader(const std::string& key, const std::string& value);
    std::string getHeader(const std::string& key) const;

    void        setBody(const std::string& body);
    std::string getBody() const;

    void setStatusCode(int code);
    int  getStatusCode() const;

    std::string buildResponse() const;

    void setMethod(Method method);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);

    Method      getMethod() const;
    std::string getPath() const;
    std::string getVersion() const;

  private:
    Method                                       method_;
    std::string                                  path_;
    std::string                                  version_;
    int                                          status_code_ = 200;
    std::unordered_map<std::string, std::string> headers_;
    std::string                                  body_;
};

#endif  // HTTP_MESSAGE_H