typedef unsigned char u8;
typedef unsigned long u32;

void write_dword(u32 value, u8 *destination)
{
    *(u32 *)destination = value;
}
