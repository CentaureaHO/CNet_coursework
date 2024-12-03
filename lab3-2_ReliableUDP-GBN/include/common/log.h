#ifndef __COMMON_LOG_H__
#define __COMMON_LOG_H__

#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <common/lock.h>

class Logger
{
  private:
    bool          logFileOpened_;
    std::ofstream logFile_;
    ReWrLock      rwLock;
    std::string   logFileName_;

    bool        openLogFile();
    std::string getCurrentTime();

  public:
    Logger(const std::string& logFileName);
    ~Logger();

    void log(const std::string& message);
};

#define LOG(logger, ...)                                                                       \
    {                                                                                          \
        std::ostringstream oss;                                                                \
        oss << "LOG: ";                                                                        \
        std::apply([&oss](auto&&... args) { ((oss << args), ...); }, std::tuple{__VA_ARGS__}); \
        logger.log(oss.str());                                                                 \
    }

#define LOG_WARN(logger, ...)                                                                  \
    {                                                                                          \
        std::ostringstream oss;                                                                \
        oss << "WARN: ";                                                                       \
        std::apply([&oss](auto&&... args) { ((oss << args), ...); }, std::tuple{__VA_ARGS__}); \
        logger.log(oss.str());                                                                 \
    }

#define LOG_ERR(logger, ...)                                                                   \
    {                                                                                          \
        std::ostringstream oss;                                                                \
        oss << "ERROR: ";                                                                      \
        std::apply([&oss](auto&&... args) { ((oss << args), ...); }, std::tuple{__VA_ARGS__}); \
        logger.log(oss.str());                                                                 \
    }

#endif