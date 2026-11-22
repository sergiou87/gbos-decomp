typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u8 *fat_table_buffer;
extern u32 fat_cluster_cursor;
extern u32 fat_table_sectors;
extern u8 fat_read_table_sector(u32 sector);
extern u8 fat_read_entry(u32 cluster, u32 *value);

u8 fat_find_free_run(u32 *start, u16 count)
{
    u32 cluster;
    u32 first;
    u8 error;
    u16 found;
    u32 sector;
    u32 value;
    u32 *last_entry;

    last_entry = (u32 *)(fat_table_buffer + 0x1fc);
    sector = fat_cluster_cursor >> 7;
    for (;;) {
        error = fat_read_table_sector(sector);
        if (error)
            return error;
        cluster = *last_entry;
        if (cluster) {
            sector += 0x40;
            if (sector >= fat_table_sectors)
                return 0xf4;
            continue;
        }
        first = sector << 7;
        if (first < 2)
            first = 2;
        cluster = first;
        found = 0;
        for (;;) {
            error = fat_read_entry(cluster, &value);
            if (error)
                return error;
            cluster++;
            if (!value) {
                found++;
                if (found == count) {
                    *start = cluster - count;
                    fat_cluster_cursor = *start;
                    return 0;
                }
            } else if (found) {
                break;
            }
        }
        sector++;
    }
}
