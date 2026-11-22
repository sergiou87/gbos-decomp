typedef unsigned char u8;

u8 bytes_equal(const void *left, const void *right, u8 length)
{
    const u8 *first = left;
    const u8 *second = right;

    while (length--)
        if (*first++ != *second++)
            return 0;
    return 1;
}
