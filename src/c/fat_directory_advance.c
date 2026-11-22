typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u8 fat_sectors_per_cluster;
extern u32 fat_cluster_to_sector(u32 cluster);
extern u8 fat_next_cluster(u32 *cluster);
extern u8 fat_allocate_chain(u32 cluster, u16 count, u8 clear);
extern u8 fat_read_sector(u32 sector);

u8 fat_advance_directory(u32 *cluster, u32 *sector, u8 *cluster_offset)
{
    u32 previous_cluster;
    u8 error;

    if (*sector == 0) {
        *cluster_offset = 0;
        *sector = fat_cluster_to_sector(*cluster);
    } else {
        *cluster_offset += 1;
        *sector += 1;
        if (*cluster_offset == fat_sectors_per_cluster) {
            *cluster_offset = 0;
            previous_cluster = *cluster;
            error = fat_next_cluster(cluster);
            if (error)
                return error;
            if (*cluster == 0x0fffffff) {
                error = fat_allocate_chain(previous_cluster, 1, 1);
                if (error)
                    return error;
                *cluster = previous_cluster;
                error = fat_next_cluster(cluster);
                if (error)
                    return error;
            }
            *sector = fat_cluster_to_sector(*cluster);
        }
    }
    error = fat_read_sector(*sector);
    return error;
}
