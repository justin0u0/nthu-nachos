#include "syscall.h"

int main()
{
    int n, i;
    for (n = 1; n < 5; ++n) {
        PrintInt(7);
        for (i=0; i<100; ++i);
    }
    Exit(7);
}
