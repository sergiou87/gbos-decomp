typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"

extern FatFileState fat_file_state;
extern u8 fat_sectors_per_cluster;
extern u8 sd_write(u32 sector, void *buffer, u16 count);
extern u8 fat_next_cluster(u32 *cluster);
extern u32 fat_cluster_to_sector(u32 cluster);

u8 fat_write_file(u16 buffer, u16 count)
{
    u8 error = 0;
    u16 chunk;

    if (fat_file_state.writing == 0)
        return FAT_ERR_FILE_MODE2;
    if (fat_file_state.remaining < count)
        return FAT_ERR_OUT_OF_FILE_WR;
    fat_file_state.remaining -= count;
    if (!fat_file_state.remaining)
        fat_file_state.writing = 0;
    while (count) {
        if (!fat_file_state.cluster_offset) {
            chunk = fat_sectors_per_cluster < count ? fat_sectors_per_cluster : count;
        } else {
            chunk = fat_sectors_per_cluster - fat_file_state.cluster_offset;
            if (chunk > count)
                chunk = count;
        }
        error = sd_write(fat_file_state.sector, (void *)buffer, chunk);
        if (error)
            return error;
        fat_file_state.cluster_offset += chunk;
        buffer += chunk * STORAGE_SECTOR_SIZE;
        count -= chunk;
        if (fat_file_state.cluster_offset == fat_sectors_per_cluster) {
            fat_file_state.cluster_offset = 0;
            error = fat_next_cluster(&fat_file_state.cluster);
            if (error)
                return error;
            fat_file_state.sector = fat_cluster_to_sector(fat_file_state.cluster);
            if (error)
                return error;
        } else {
            fat_file_state.sector += chunk;
        }
    }
    return 0;
}
