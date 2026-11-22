typedef unsigned char u8;
typedef unsigned int u16;

extern u8 os_byte;

u8 os_store_byte(u16 value)
{
    os_byte = value;
    return 1;
}
