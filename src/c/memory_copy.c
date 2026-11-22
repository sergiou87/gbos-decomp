typedef unsigned char u8;
typedef unsigned int u16;

void memory_copy(const u8 *source, u8 *destination, u16 count)
{
    u8 *output;
    const u8 *input;

    input = source;
    output = destination;
    while (count--)
        *output++ = *input++;
}
