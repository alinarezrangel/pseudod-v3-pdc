int main(void)
{
    _Static_assert(sizeof(void*) == 8, "ptr is not 64 bits");
    return 0;
}
