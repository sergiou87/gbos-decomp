typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"
#include "storage_layout.h"
#include "error.h"

extern FatFileState fat_file_state;
extern u8 fat_sectors_per_cluster;
extern u8 sd_read(u32 sector, void *buffer, u16 count);
extern u8 sd_read_cartridge(u32 sector, void *address, u16 count);
extern u8 fat_next_cluster(u32 *cluster);
extern u32 fat_cluster_to_sector(u32 cluster);

u8 fat_read_file(u8 *buffer, u16 count, u8 mode)
{
    u8 error = 0;
    u16 chunk = 0;

    if (fat_file_state.writing)
        return FAT_ERR_FILE_MODE1;
    if (fat_file_state.remaining < count)
        return FAT_ERR_OUT_OF_FILE;
    fat_file_state.remaining -= count;
    while (count) {
        if (!fat_file_state.cluster_offset) {
            chunk = fat_sectors_per_cluster < count ? fat_sectors_per_cluster : count;
        } else {
            chunk = fat_sectors_per_cluster - fat_file_state.cluster_offset;
            if (chunk > count)
                chunk = count;
        }
        if (!mode) {
            error = sd_read_cartridge(fat_file_state.sector, buffer, chunk);
            *(u16 *)buffer += chunk;
        } else {
            error = sd_read(fat_file_state.sector, buffer, chunk);
            buffer += chunk * STORAGE_SECTOR_SIZE;
        }
        if (error)
            return error;
        fat_file_state.cluster_offset += chunk;
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
