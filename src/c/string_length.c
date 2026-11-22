typedef unsigned char u8;

u8 string_length(const u8 *value)
{
    u8 length = 0;
    while (*value++)
        length++;
    return length;
}
