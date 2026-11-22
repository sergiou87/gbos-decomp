typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u32 fat_entry_count;
extern u32 fat_table_start;
extern u32 fat_volume_start;
extern u32 fat_cached_sector;
extern u32 fat_cached_table_sector;
extern u32 fat_table_sectors;
extern u32 fat_root_sector;
extern u32 fat_data_start;
extern u16 fat_reserved_sectors;
extern u8 fat_sectors_per_cluster;
extern u8 fat_type;
extern u8 fat_cluster_shift;
extern u32 fat_cluster_cursor;
extern u16 fat_sector_buffer;
extern u8 *fat_table_buffer;
extern u16 fat_work_buffer;

extern u16 heap_alloc(u16 size);
extern u8 fat_read_sector(u32 sector);
extern u8 fat_detect_type(void);
extern u16 fat_read_word(const u8 *buffer);
extern void read_dword(const u8 *buffer, u32 *value);

u8 fat_initialize(void)
{
    u8 value;

    fat_sector_buffer = heap_alloc(0x200);
    fat_table_buffer = (u8 *)heap_alloc(0x200);
    fat_work_buffer = heap_alloc(0x1000);
    fat_cached_sector = 0xffffffffUL;
    fat_cached_table_sector = 0xffffffffUL;
    fat_volume_start = 0;
    value = fat_read_sector(0);
    if (value)
        return value;
    fat_type = fat_detect_type();
    if (!fat_type) {
        read_dword((const u8 *)(fat_sector_buffer + 0x1c6), &fat_volume_start);
        value = fat_read_sector(fat_volume_start);
        if (value)
            return value;
        fat_type = fat_detect_type();
        if (!fat_type)
            return 0xf6;
    }
    if (fat_type == 1)
        return 0xfb;

    read_dword((const u8 *)(fat_sector_buffer + 0x24), &fat_table_sectors);
    fat_entry_count = fat_table_sectors << 7;
    fat_reserved_sectors = fat_read_word((const u8 *)(fat_sector_buffer + 0x0e));
    fat_sectors_per_cluster = *(const u8 *)(fat_sector_buffer + 0x0d);
    fat_root_sector = (fat_table_sectors << 1) + fat_volume_start + fat_reserved_sectors;
    fat_table_start = fat_volume_start + fat_reserved_sectors;
    fat_data_start = fat_root_sector;
    value = fat_sectors_per_cluster;
    fat_cluster_shift = 0;
    while (value > 1) {
        value >>= 1;
        fat_cluster_shift++;
    }
    fat_cluster_cursor = 2;
    return 0;
}
