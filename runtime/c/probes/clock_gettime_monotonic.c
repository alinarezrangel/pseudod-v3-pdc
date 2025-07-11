#include <time.h>

int main(void)
{
    struct timespec res;
    int ok = clock_gettime(CLOCK_MONOTONIC, &res);
    (void) ok;
    return 0;
}
