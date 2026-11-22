typedef unsigned char u8;
typedef unsigned long u32;

extern u32 fat_data_start;
extern u8 fat_cluster_shift;
extern u8 fat_sectors_per_cluster;
extern u32 fat_sector_scratch;
extern u32 fat_cluster_scratch;

u32 fat_cluster_to_sector(u32 cluster)
{
    fat_sector_scratch = cluster - 2;
    fat_sector_scratch <<= fat_cluster_shift;
    fat_sector_scratch += fat_data_start;
    return fat_sector_scratch;
}

u32 fat_sector_to_cluster(u32 sector)
{
    fat_cluster_scratch = sector - fat_data_start;
    fat_cluster_scratch >>= fat_cluster_shift;
    fat_cluster_scratch += 2;
    return fat_cluster_scratch;
}

u8 fat_sector_in_cluster(u32 sector)
{
    sector -= fat_data_start;
    return sector & (u8)(fat_sectors_per_cluster - 1);
}
