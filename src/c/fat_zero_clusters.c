typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u32 fat_cached_sector;
extern u16 fat_sector_buffer;
extern u8 fat_sectors_per_cluster;
extern u32 fat_cluster_to_sector(u32 cluster);
extern void memory_fill(void *destination, u8 value, u16 count);
extern u8 sd_write(u32 sector, void *buffer, u16 count);

u8 fat_zero_clusters(u32 cluster, u16 count)
{
    u32 sector;
    u8 index;
    u8 error = 0;

    sector = fat_cluster_to_sector(cluster);
    fat_cached_sector = 0xffffffff;
    memory_fill((void *)fat_sector_buffer, 0, 512);
    while (count--) {
        for (index = 0; index < fat_sectors_per_cluster; index++) {
            error = sd_write(sector, (void *)fat_sector_buffer, 1);
            if (error)
                return error;
            sector++;
        }
    }
    return 0;
}
