typedef unsigned char u8;
typedef unsigned long u32;

extern u8 fat_read_entry(u32 cluster, u32 *value);

u8 fat_next_cluster(u32 *cluster)
{
    return fat_read_entry(*cluster, cluster);
}
