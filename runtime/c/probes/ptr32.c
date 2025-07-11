int main(void)
{
    _Static_assert(sizeof(void*) == 4, "ptr is not 32 bits");
    return 0;
}
