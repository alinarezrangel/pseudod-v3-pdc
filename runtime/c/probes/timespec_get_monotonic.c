#include <time.h>

int main(void)
{
    struct timespec res;
    int ok = timespec_get(&res, TIME_MONOTONIC);
    (void) ok;
    return 0;
}
