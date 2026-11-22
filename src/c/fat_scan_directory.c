

#include "fat.h"
#include "disk.h"
#include "str.h"
#include "os.h"
#include "error.h"
#include "sys.h"

extern u32 fat_parent_cluster;
extern u16 fat_directory_count;
extern u8 fat_directory_is_root;
extern u32 fat_root_sector;
extern u8 fat_sectors_per_cluster;
extern u16 fat_sector_buffer;
extern u32 *fat_work_buffer;
extern u32 fat_cluster_to_sector(u32 cluster);
extern u8 fat_read_sector(u32 sector);
extern u16 fat_read_word(const u8 *buffer);
extern u8 fat_next_cluster(u32 *cluster);

u8 fat_scan_directory(u32 cluster)
{
    u32 start;
    u8 long_name;
    u8 sector_index;
    u32 sector_shift;
    u8 entry_index;
    u8 error;
    u8 *entry;
    u8 character;
    u8 root_count;

    root_count = 0;
    long_name = 0;
    character = 0;
    fat_directory_count = 0;
    if (!cluster) {
        start = fat_root_sector;
        cluster = 2;
        fat_directory_is_root = 1;
        fat_parent_cluster = 0;
    } else {
        fat_directory_is_root = 0;
        start = fat_cluster_to_sector(cluster);
        error = fat_read_sector(start);
        if (error)
            return error;
        fat_parent_cluster = fat_read_word((const u8 *)(fat_sector_buffer + FAT_PARENT_CLUSTER_HIGH_OFFSET));
        fat_parent_cluster <<= 16;
        fat_parent_cluster |= fat_read_word((const u8 *)(fat_sector_buffer + FAT_PARENT_CLUSTER_LOW_OFFSET));
    }
    for (;;) {
        for (sector_index = 0; sector_index < fat_sectors_per_cluster; ++sector_index) {
            sector_shift = start << FAT_ENTRY_LOCATION_SHIFT;
            error = fat_read_sector(start);
            if (error)
                return error;
            entry = (u8 *)fat_sector_buffer;
            for (entry_index = 0; entry_index < FAT_ENTRIES_PER_SECTOR;
                 ++entry_index, entry += FAT_DIRECTORY_ENTRY_SIZE) {
                if (*entry == 0)
                    return 0;
                if (*entry == FAT_ENTRY_DELETED || *entry == '.') {
                    long_name = 0;
                    continue;
                }
                if (entry[FAT_ENTRY_ATTRIBUTE_OFFSET] == FAT_ATTRIBUTE_LONG_NAME) {
                    character = entry[1];
                    if ((*entry & FAT_LFN_SEQUENCE_MASK) == FAT_LFN_LAST_ENTRY) {
                        long_name = 1;
                        fat_work_buffer[fat_directory_count] = sector_shift | entry_index;
                    }
                    continue;
                }
                if ((character == '.' && long_name) ||
                    (entry[FAT_ENTRY_ATTRIBUTE_OFFSET] &
                     FAT_ATTRIBUTE_SKIP_BROWSER)) {
                    long_name = 0;
                    continue;
                }
                if (!long_name)
                    fat_work_buffer[fat_directory_count] = sector_shift | entry_index;
                fat_directory_count++;
                long_name = 0;
                if (fat_directory_count == FAT_MAX_DIRECTORY_ENTRIES)
                    return 0;
            }
            start++;
        }
        error = fat_next_cluster(&cluster);
        if (error)
            return error;
        start = fat_cluster_to_sector(cluster);
    }
}
