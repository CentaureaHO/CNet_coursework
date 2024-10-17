#ifndef __COMMON_CONSOLE_FORMAT_DATE_H__
#define __COMMON_CONSOLE_FORMAT_DATE_H__

#include <string>

#define DLOG std::cout << formatDate() << " "
#define DERR std::cerr << formatDate() << " "

std::string formatDate();

#endif