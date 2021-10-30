#include "syscall.h"

int main()
{
    int n;
    for (n = 1; n < 10000; ++n) {}
    PrintInt(2);
    for (n = 1; n < 10000; ++n) {}
    Exit(2);
}
