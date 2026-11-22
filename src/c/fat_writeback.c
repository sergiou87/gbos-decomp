typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u32 fat_cached_table_sector;
extern u32 fat_table_start;
extern u32 fat_table_sectors;
extern u8 *fat_table_buffer;
extern u32 fat_cached_sector;
extern u16 fat_sector_buffer;
extern u8 sd_write(u32 sector, void *buffer, u16 count);

u8 fat_write_table_sector(void)
{
    u8 error;
    u32 sector;

    sector = fat_cached_table_sector + fat_table_start;
    error = sd_write(sector, fat_table_buffer, 1);
    if (error)
        return error;
    error = sd_write(sector + fat_table_sectors, fat_table_buffer, 1);
    return error;
}

u8 fat_write_cached_sector(void)
{
    return sd_write(fat_cached_sector, (void *)fat_sector_buffer, 1);
}
