typedef unsigned char u8;
typedef unsigned int u16;

extern u16 fat_sector_buffer;
extern u8 bytes_equal(const void *left, const void *right, u8 length);

u8 fat_detect_type(void)
{
    if (bytes_equal((const u8 *)(fat_sector_buffer + 0x36), (const u8 *)"FAT", 3))
        return 1;
    if (bytes_equal((const u8 *)(fat_sector_buffer + 0x52), (const u8 *)"FAT", 3))
        return 2;
    return 0;
}

u16 fat_read_word(const u8 *buffer)
{
    return buffer[0] | ((u16)buffer[1] << 8);
}
