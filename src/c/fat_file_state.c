typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"
#include "storage_layout.h"
#include "error.h"

extern FatFileState fat_file_state;
extern u8 fat_sectors_per_cluster;
extern u8 fat_extend_file(FatFile *file, u16 sectors);
extern u8 fat_load_metadata(u32 entry, FatFile *file);
extern u32 fat_cluster_to_sector(u32 cluster);
extern u8 fat_next_cluster(u32 *cluster);

u8 fat_open_file(FatFile *file, u16 sectors)
{
    u8 error;

    if (file->is_directory)
        return FAT_ERR_NOT_FILE;
    fat_file_state.size = file->size;
    fat_file_state.remaining = fat_file_state.size / STORAGE_SECTOR_SIZE;
    if (fat_file_state.size % STORAGE_SECTOR_SIZE)
        fat_file_state.remaining++;
    if (fat_file_state.remaining < sectors) {
        error = fat_extend_file(file, sectors);
        if (error)
            return error;
        error = fat_load_metadata(file->first_entry, file);
        if (error)
            return error;
    }
    fat_file_state.cluster = file->cluster;
    fat_file_state.sector = fat_cluster_to_sector(fat_file_state.cluster);
    fat_file_state.cluster_offset = 0;
    fat_file_state.size = file->size;
    fat_file_state.writing = sectors ? 1 : 0;
    fat_file_state.remaining = fat_file_state.size / STORAGE_SECTOR_SIZE;
    if (fat_file_state.size % STORAGE_SECTOR_SIZE)
        fat_file_state.remaining++;
    return 0;
}

u8 fat_advance_file(u16 sectors)
{
    u16 remaining;
    u8 error;

    if (fat_file_state.remaining < sectors)
        return FAT_ERR_OUT_OF_FILE;
    fat_file_state.remaining -= sectors;
    remaining = sectors;
    while (remaining) {
        fat_file_state.sector++;
        fat_file_state.cluster_offset++;
        remaining--;
        if (fat_file_state.cluster_offset == fat_sectors_per_cluster) {
            fat_file_state.cluster_offset = 0;
            error = fat_next_cluster(&fat_file_state.cluster);
            if (error)
                return error;
            fat_file_state.sector = fat_cluster_to_sector(fat_file_state.cluster);
        }
    }
    return 0;
}
