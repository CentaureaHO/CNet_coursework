#include <ctime>
#include <iomanip>
#include <sstream>

#include <common/console/format_date.h>
using namespace std;

string formatDate()
{
    ostringstream oss;
    time_t        t = time(nullptr);
    tm            tm_struct;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    oss << '<' << setfill('0') << setw(2) << (tm_struct.tm_year % 100) << ':' << setw(2) << (tm_struct.tm_mon + 1)
        << ':' << setw(2) << tm_struct.tm_mday << ':' << setw(2) << tm_struct.tm_hour << ':' << setw(2)
        << tm_struct.tm_min << ':' << setw(2) << tm_struct.tm_sec << '>';
    return oss.str();
}