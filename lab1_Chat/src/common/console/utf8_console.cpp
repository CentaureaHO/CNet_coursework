#include <iostream>
#include <cassert>

#include <common/console/utf8_console.h>

void u8console_init()
{
#ifdef _WIN32
    utf8_win::u8console_init_windows();
#endif
}

int unicodeHandler(unsigned int codepoint, char* output)
{
    assert(output != nullptr);

    if (codepoint <= 0x7F)
    {
        output[0] = static_cast<char>(codepoint);
        return 1;
    }
    else if (codepoint <= 0x7FF)
    {
        output[0] = static_cast<char>((codepoint >> 6) | 0xC0);
        output[1] = static_cast<char>((codepoint & 0x3F) | 0x80);
        return 2;
    }
    else if (codepoint <= 0xFFFF)
    {
        output[0] = static_cast<char>((codepoint >> 12) | 0xE0);
        output[1] = static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80);
        output[2] = static_cast<char>((codepoint & 0x3F) | 0x80);
        return 3;
    }
    else if (codepoint <= 0x10FFFF)
    {
        output[0] = static_cast<char>((codepoint >> 18) | 0xF0);
        output[1] = static_cast<char>(((codepoint >> 12) & 0x3F) | 0x80);
        output[2] = static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80);
        output[3] = static_cast<char>((codepoint & 0x3F) | 0x80);
        return 4;
    }
    return 0;
}

void utf8Encode(std::string& input)
{
    size_t writeIndex = 0;
    size_t readIndex  = 0;

    while (readIndex < input.size())
    {
        if (input[readIndex] == '\\' && readIndex + 1 < input.size() && input[readIndex + 1] == 'u')
        {
            readIndex += 2;
            if (readIndex + 4 <= input.size())
            {
                std::string  hexStr    = input.substr(readIndex, 4);
                unsigned int codepoint = std::stoul(hexStr, nullptr, 16);
                readIndex += 4;

                char utf8Buf[4];
                int  utf8Len = unicodeHandler(codepoint, utf8Buf);
                for (int i = 0; i < utf8Len; ++i) { input[writeIndex++] = utf8Buf[i]; }
            }
            else
            {
                input[writeIndex++] = '\\';
                input[writeIndex++] = 'u';
            }
        }
        else
            input[writeIndex++] = input[readIndex++];
    }
    input.resize(writeIndex);
}

U8ConsoleInit::U8ConsoleInit() { u8console_init(); }

static U8ConsoleInit utf8_console_initializer;

std::string unicodeToUTF8(unsigned int codepoint)
{
    std::string utf8str;

    if (codepoint <= 0x7F) { utf8str.push_back(static_cast<char>(codepoint)); }
    else if (codepoint <= 0x7FF)
    {
        utf8str.push_back(static_cast<char>((codepoint >> 6) | 0xC0));
        utf8str.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
    }
    else if (codepoint <= 0xFFFF)
    {
        utf8str.push_back(static_cast<char>((codepoint >> 12) | 0xE0));
        utf8str.push_back(static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80));
        utf8str.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
    }
    else if (codepoint <= 0x10FFFF)
    {
        utf8str.push_back(static_cast<char>((codepoint >> 18) | 0xF0));
        utf8str.push_back(static_cast<char>(((codepoint >> 12) & 0x3F) | 0x80));
        utf8str.push_back(static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80));
        utf8str.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
    }

    return utf8str;
}

U8In::U8In() : std::istream(std::cin.rdbuf()) {}

U8In& U8In::getInstance()
{
    static U8In instance;
    return instance;
}

U8In& u8in = U8In::getInstance();

std::istream& operator>>(U8In& u8in, std::string& str)
{
    str.clear();
    char ch;

    while (std::cin.get(ch))
    {
        if (ch == '\\' && std::cin.peek() == 'u')
        {
            std::cin.get(ch);
            std::string hexStr;
            for (int i = 0; i < 5 && std::cin.get(ch); ++i)
            {
                if (isxdigit(ch))
                    hexStr.push_back(ch);
                else
                    break;
            }

            unsigned int       codepoint;
            std::istringstream iss(hexStr);
            iss >> std::hex >> codepoint;

            if (!iss.fail()) { str += unicodeToUTF8(codepoint); }
            else { str += "\\u" + hexStr; }
        }
        else { str.push_back(ch); }

        if (ch == '\n') { break; }
    }

    if (!str.empty() && str.back() == '\n') { str.pop_back(); }
    if (!str.empty() && str.back() == '\r') { str.pop_back(); }

    return std::cin;
}

#ifdef _WIN32
namespace utf8_win
{
    u8streambuf::u8streambuf(unsigned int read_buf_size, bool handle_console_eof)
        : _read_buf_size(read_buf_size),
          _u8_buf_size(read_buf_size * 6),
          _wide_buf_size(read_buf_size * 2),
          _buffer(new char[_u8_buf_size]),
          _wide_buffer(new wchar_t[_wide_buf_size]),
          _handle_eof(handle_console_eof),
          _eof(false)
    {}

    u8streambuf::~u8streambuf()
    {
        delete[] _buffer;
        delete[] _wide_buffer;
    }

    u8streambuf& u8streambuf::getInstance(
        unsigned int read_buf_size = U8_READ_BUFFER_SIZE, bool handle_console_eof = false)
    {
        static u8streambuf instance(read_buf_size, handle_console_eof);
        return instance;
    }

    std::streambuf::int_type u8streambuf::underflow()
    {
        if (_eof)
        {
            setg(nullptr, nullptr, nullptr);
            return std::streambuf::traits_type::eof();
        }

        if (gptr() < egptr()) return std::streambuf::traits_type::to_int_type(*this->gptr());

        unsigned long read_len;
        bool          ret = ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), _wide_buffer, _wide_buf_size, &read_len, NULL);

        if (!ret || read_len == 0)
        {
            _eof = true;
            setg(nullptr, nullptr, nullptr);
            return std::streambuf::traits_type::eof();
        }

        if (_handle_eof)
        {
            for (unsigned long i = 0; i < read_len; i++)
            {
                if (_wide_buffer[i] == 0x001A)
                {
                    read_len = i;
                    _eof     = true;
                }
            }
        }

        if (read_len == 0)
        {
            _eof = true;
            setg(nullptr, nullptr, nullptr);
            return std::streambuf::traits_type::eof();
        }

        int size = WideCharToMultiByte(CP_UTF8, 0, _wide_buffer, read_len, _buffer, _u8_buf_size, NULL, NULL);
        setg(_buffer, _buffer, _buffer + size);

        if (size == 0) return std::streambuf::traits_type::eof();
        return std::streambuf::traits_type::to_int_type(*this->gptr());
    }

    void u8console_init_windows()
    {
        SetConsoleOutputCP(65001);
        SetConsoleCP(65001);

        std::cin.rdbuf(&u8streambuf::getInstance(U8_READ_BUFFER_SIZE, true));
    }
}  // namespace utf8_win
#endif