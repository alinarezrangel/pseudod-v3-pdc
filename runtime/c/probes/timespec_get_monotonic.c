#include <stdio.h>
#include <time.h>

int main(void)
{
    struct timespec res;
    int ok = timespec_get(&res, TIME_MONOTONIC);
    if(ok != 0)
        puts("#define PDCRT_OS_TIMESPEC_GET_MONOTONIC");
    return 0;
}
