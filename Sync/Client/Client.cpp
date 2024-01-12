#include <Windows.h>
#include <iostream>
#include <thread>
#include <chrono>

#include "Registry.h"
#include "Driver.h"

int main()
{
    if (!Driver::InitComm(14132))
        return 1;

    int value = 14132;

    std::uint64_t Base = Driver::GetBase();
    printf("Base: 0x%llx\n", Base);

    while (true)
    {
        int ReadValue = Driver::ReadMem<int>(0xC04AFF760);
        printf("%i\n", ReadValue);

    }
}