typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

void read_dword(const u8 *buffer, u32 *value)
{
    u32 *destination = value;
    const u32 *source = (const u32 *)buffer;

    *destination = *source;
}
