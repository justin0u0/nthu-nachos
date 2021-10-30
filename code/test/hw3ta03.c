#include "syscall.h"

int main()
{
    int n;
    for (n = 1; n < 100; ++n) {}
    PrintInt(3);
    for (n = 1; n < 20000; ++n) {}
    Exit(3);
}
