typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"

extern u16 fat_directory_count;
extern u32 *fat_work_buffer;
extern u8 fat_scan_directory(u32 cluster);
extern u8 fat_load_metadata(u32 entry, FatFile *file);
extern u8 bytes_equal(const void *left, const void *right, u8 count);

u8 fat_find_path(u8 *path, FatFile *volatile file, u32 *parent, u8 directory)
{
    u32 *parent_output;
    FatFile *descriptor;
    const u8 *directory_field;
    const u32 *cluster_field;
    u8 character;
    u8 padding;
    u8 length;
    u32 cluster;
    u8 matches;
    u8 found;
    u8 error;
    u16 index;
    u8 last;

    last = 0;
    cluster = 0;
    found = 0;
    parent_output = parent;
    *parent_output = 0x0fffffff;
    descriptor = file;
    directory_field = &descriptor->is_directory;
    cluster_field = &descriptor->cluster;
    do {
        if (*path == '/')
            path++;
        for (length = 0; path[length] != '/' && path[length]; length++)
            ;
        if (!path[length])
            last = 1;
        error = fat_scan_directory(cluster);
        if (error)
            return error;
        for (index = 0; index < fat_directory_count; index++) {
            error = fat_load_metadata(fat_work_buffer[index], file);
            if (error)
                return error;
            if (!*directory_field && !last)
                continue;
            matches = 1;
            for (padding = length; (character = descriptor->name[padding]); padding++) {
                if (character != ' ') {
                    matches = 0;
                    break;
                }
            }
            if (!matches)
                continue;
            if (!bytes_equal(path, descriptor->name, length))
                continue;
            if (last) {
                if (*directory_field && !directory)
                    continue;
                if (!*directory_field && directory)
                    continue;
                found = 1;
            } else {
                cluster = *cluster_field;
            }
            path += length;
            break;
        }
    } while (!error && !last && index != fat_directory_count);
    if (last)
        *parent_output = cluster;
    if (!found)
        return 0xf0;
    return 0;
}
