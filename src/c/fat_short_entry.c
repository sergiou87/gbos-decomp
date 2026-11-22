typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u16 fat_sector_buffer;
extern u16 fat_default_date(void);
extern u16 fat_default_time(void);
extern void write_dword(u32 value, u8 *destination);
extern u8 fat_write_cached_sector(void);

u8 fat_write_short_entry(const u8 *name, u8 slot, u8 attributes, u32 cluster, u32 size)
{
    u8 index;
    u8 *entry;
    u16 date = fat_default_date();
    u16 time = fat_default_time();

    entry = (u8 *)(fat_sector_buffer + slot * 32);
    for (index = 0; index < 11; index++)
        *entry++ = name[index];
    *entry++ = attributes;
    for (index = 0; index < 8; index++)
        entry++;
    *entry++ = (cluster >> 16) & 0xff;
    *entry++ = (cluster >> 24) & 0xff;
    *entry++ = time & 0xff;
    *entry++ = time >> 8;
    *entry++ = date & 0xff;
    *entry++ = date >> 8;
    *entry++ = cluster & 0xff;
    *entry++ = (cluster >> 8) & 0xff;
    write_dword(size, entry);
    return fat_write_cached_sector();
}
