typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"

extern const u8 fat_directory_blank_name[12];
extern u8 fat_sectors_per_cluster;
extern u16 fat_sector_buffer;
extern u8 fat_find_path(const u8 *path, FatFile *file, u32 *parent, u8 directory);
extern u8 fat_create_path_entry(const u8 *path, u32 parent, u16 sectors, u8 directory);
extern u32 fat_cluster_to_sector(u32 cluster);
extern u8 fat_read_sector(u32 sector);
extern u8 fat_write_short_entry(const u8 *name, u8 slot, u8 attributes, u32 cluster, u32 size);
extern u8 fat_write_cached_sector(void);

u8 fat_make_directory(FatFile *file, const u8 *path)
{
    u32 *cluster;
    u8 error = 0;
    u32 parent;

    error = fat_find_path(path, file, &parent, 1);
    if (!error)
        return 0xf1;
    if (error == 0xf0) {
        if (parent == 0x0fffffff)
            return 0xfa;
        error = fat_create_path_entry(path, parent, fat_sectors_per_cluster, 1);
        if (error)
            return error;
        error = fat_find_path(path, file, &parent, 1);
        if (error == 0xf0)
            return 0xff;
        if (error)
            return error;
        cluster = &file->cluster;
        error = fat_read_sector(fat_cluster_to_sector(*cluster));
        if (error)
            return error;
        fat_write_short_entry(fat_directory_blank_name, 0, 0x10, *cluster, 0);
        fat_write_short_entry(fat_directory_blank_name, 1, 0x10, parent, 0);
        *(u8 *)fat_sector_buffer = '.';
        *(u8 *)(fat_sector_buffer + 32) = '.';
        *(u8 *)(fat_sector_buffer + 33) = '.';
        error = fat_write_cached_sector();
        if (error)
            return error;
    }
    return error;
}
