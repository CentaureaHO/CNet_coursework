#include <bits/stdc++.h>
#include <common/log.h>

int main()
{
    Logger logger("application.log");
    Logger logger2("application2.log");

    LOG(logger, "程序启动，版本：", "1.0.0");
    LOG(logger, "处理任务1，文件大小：", 500, " KB");

    LOG_ERR(logger, "文件读取失败，错误码：", -1);

    LOG(logger2, "程序启动，版本：", "1.0.0");

    int i = 0;
    std::cin >> i;

    return 0;
}