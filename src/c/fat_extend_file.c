typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"

extern u16 fat_sector_buffer;
extern u8 fat_cluster_shift;
extern u8 fat_sectors_per_cluster;
extern u8 fat_read_sector(u32 sector);
extern void write_dword(u32 value, u8 *destination);
extern u8 fat_write_cached_sector(void);
extern u8 fat_last_cluster(u32 cluster, u32 *last);
extern u8 fat_allocate_chain(u32 last, u16 count, u8 clear);

u8 fat_extend_file(FatFile *file, u16 sectors)
{
    u16 old_sectors;
    u32 size;
    u8 slot;
    u16 old_clusters;
    u8 error;
    u32 last;
    u16 clusters;

    size = (u32)sectors << 9;
    if (file->size >= size)
        return 0xfc;
    slot = file->entry & 0x0f;
    error = fat_read_sector(file->entry >> 4);
    if (error)
        return error;
    write_dword(size, (u8 *)(fat_sector_buffer + slot * 32 + 0x1c));
    error = fat_write_cached_sector();
    if (error)
        return error;
    old_sectors = file->size >> 9;
    if (file->size & 0x1ff)
        old_sectors++;
    old_clusters = old_sectors >> fat_cluster_shift;
    if (old_sectors & (fat_sectors_per_cluster - 1))
        old_clusters++;
    clusters = sectors >> fat_cluster_shift;
    if (sectors & (fat_sectors_per_cluster - 1))
        clusters++;
    if (old_clusters == clusters)
        return 0;
    clusters -= old_clusters;
    error = fat_last_cluster(file->cluster, &last);
    if (error)
        return error;
    return fat_allocate_chain(last, clusters, 0);
}
