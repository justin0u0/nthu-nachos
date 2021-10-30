#include "syscall.h"

int main()
{
    int n2, n, i;
    unsigned long long x = 0;

    for (n = 0; n < 5; ++n) {
        for (i=0; i<2; ++i) x += 1;
    }
    Exit(1);
}
