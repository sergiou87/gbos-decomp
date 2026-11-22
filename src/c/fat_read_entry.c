typedef unsigned char u8;
typedef unsigned long u32;

extern u8 *fat_table_buffer;
extern u8 fat_read_table_sector(u32 sector);
extern void read_dword(const u8 *buffer, u32 *value);

u8 fat_read_entry(u32 cluster, u32 *value)
{
    u8 error;
    u32 offset;

    error = fat_read_table_sector(cluster >> 7);
    if (error)
        return error;
    offset = (cluster & 0x7f) << 2;
    read_dword(fat_table_buffer + offset, value);
    return 0;
}
