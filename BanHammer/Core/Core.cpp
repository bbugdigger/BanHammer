#include <spdlog/spdlog.h>
#include <iostream>

int main()
{
    spdlog::info("BanHammer Started...");

    __debugbreak();
    return 0;
}
