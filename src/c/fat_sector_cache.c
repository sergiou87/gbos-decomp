typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

__at(0xc012) u32 fat_cached_sector;
__at(0xc036) u16 fat_sector_buffer;

extern u32 fat_table_start;
extern u32 fat_cached_table_sector;
extern u8 *fat_table_buffer;
extern u8 sd_read(u32 sector, void *buffer, u16 count);

u8 fat_read_sector(u32 sector)
{
    if (sector == fat_cached_sector)
        return 0;
    fat_cached_sector = sector;
    return sd_read(sector, (void *)fat_sector_buffer, 1);
}

u8 fat_read_table_sector(u32 sector)
{
    if (sector == fat_cached_table_sector)
        return 0;
    fat_cached_table_sector = sector;
    sector += fat_table_start;
    return sd_read(sector, fat_table_buffer, 1);
}
