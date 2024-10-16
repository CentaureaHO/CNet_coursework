#ifndef __UTF8_CONSOLE_H__
#define __UTF8_CONSOLE_H__

#ifndef U8_READ_BUFFER_SIZE
#define U8_READ_BUFFER_SIZE 8192
#endif

#include <sstream>
#include <string>

#ifdef _WIN32
#include <streambuf>
#include <Windows.h>
#endif

/**
 * @brief 初始化 UTF-8 控制台以正确处理编码。
 */
void u8console_init();

/**
 * @brief 将 Unicode 码点转换为 UTF-8 编码的字符串。
 *
 * @param codepoint 要转换的 Unicode 码点。
 */
void utf8Encode(std::string& input);

/**
 * @brief 确保在应用程序启动时初始化 UTF-8 控制台。
 */
class U8ConsoleInit
{
  public:
    /**
     * @brief 构造函数，初始化 UTF-8 控制台。
     */
    U8ConsoleInit();
};

/**
 * @brief 将 Unicode 码点转换为 UTF-8 编码的字符串。
 *
 * @param codepoint 要转换的 Unicode 码点。
 * @return 表示该码点的 UTF-8 编码字符串。
 */
std::string unicodeToUTF8(unsigned int codepoint);

/**
 * @brief UTF-8 控制台输入流的单例类。
 */
class U8In : public std::istream
{
  private:
    /**
     * @brief 私有构造函数，强制使用单例模式。
     */
    U8In();

  public:
    /**
     * @brief 删除的拷贝构造函数，防止复制。
     */
    U8In(const U8In&) = delete;
    /**
     * @brief 删除的赋值操作符，防止复制。
     */
    U8In& operator=(const U8In&) = delete;

    /**
     * @brief 获取 U8In 的单例实例。
     *
     * @return U8In 的单例实例的引用。
     */
    static U8In& getInstance();

    /**
     * @brief 重载输入流操作符以读取 UTF-8 字符串。
     *
     * @param u8in U8In 实例的引用。
     * @param str 用于存储输入的字符串的引用。
     * @return 输入流的引用。
     */
    friend std::istream& operator>>(U8In& u8in, std::string& str);
};

#ifdef _WIN32
/*
 * @ref: https://github.com/cqjjjzr/utf8-console
 */
namespace utf8_win
{
    /**
     * @brief 用于 Windows 上 UTF-8 输入的流缓冲区。
     */
    class u8streambuf : public std::streambuf
    {
      private:
        unsigned int _read_buf_size, _u8_buf_size, _wide_buf_size;
        char*        _buffer;
        wchar_t*     _wide_buffer;

        bool _handle_eof, _eof;

      private:
        /**
         * @brief u8streambuf 的构造函数。
         *
         * @param read_buf_size 读取缓冲区的大小。
         * @param handle_console_eof 是否处理控制台 EOF 的标志。
         */
        u8streambuf(unsigned int read_buf_size, bool handle_console_eof);

        /**
         * @brief u8streambuf 的析构函数，清理分配的缓冲区。
         */
        ~u8streambuf();

      protected:
        /**
         * @brief underflow 处理器，从控制台读取数据。
         *
         * @return 要读取的下一个字符或 EOF。
         */
        int_type underflow() override;

      public:
        /**
         * @brief 获取 u8streambuf 的单例实例。
         *
         * @param read_buf_size 读取缓冲区的大小（默认是 U8_READ_BUFFER_SIZE）。
         * @param handle_console_eof 是否处理控制台 EOF 的标志。
         * @return u8streambuf 的单例实例的引用。
         */
        static u8streambuf& getInstance(unsigned int read_buf_size, bool handle_console_eof);

        /**
         * @brief 删除的拷贝构造函数，防止复制。
         */
        u8streambuf(const u8streambuf&) = delete;
        /**
         * @brief 删除的赋值操作符，防止复制。
         */
        u8streambuf& operator=(const u8streambuf&) = delete;

        unsigned int getBufferSize() const { return _read_buf_size; }
    };

    /**
     * @brief 初始化 Windows 上的 UTF-8 控制台，设置适当的代码页。
     */
    void u8console_init_windows();
}  // namespace utf8_win
#endif

/**
 * @brief 指向 U8In 单例实例的全局引用，用于 UTF-8 控制台输入。
 */
extern U8In& u8in;

#endif