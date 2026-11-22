typedef unsigned char u8;
typedef unsigned int u16;

void memory_fill(void *destination, u8 value, u16 count)
{
    u8 *bytes = destination;

    while (count--)
        *bytes++ = value;
}
