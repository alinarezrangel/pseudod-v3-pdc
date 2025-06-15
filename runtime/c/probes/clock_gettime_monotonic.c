#include <stdio.h>
#include <time.h>

int main(void)
{
    struct timespec res;
    int ok = clock_gettime(CLOCK_MONOTONIC, &res);
    if(ok == 0)
        puts("#define PDCRT_OS_CLOCK_GETTIME_MONOTONIC");
    return 0;
}
