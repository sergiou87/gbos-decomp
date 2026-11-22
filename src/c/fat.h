/* 
 * File:   fat_.h
 * Author: krik
 *
 * Created on November 11, 2013, 8:55 PM
 */

#ifndef FAT__H
#define	FAT__H

#include "types.h"

#define FAT_TYPE_16 1
#define FAT_TYPE_32 2
#define FAT_END_VAL 0x0fffffff

typedef struct {
    u8 name[212];
    u32 lfn_entry;
    u32 short_entry;
    u32 cluster_id;
    u32 size;
    u8 is_dir;
} FatFullRecord;

typedef struct {
    u32 cluster;
    u32 sector;
    u32 size;
    u16 sectors_avail;
    u8 writing;
    u8 sector_pos;

} File;

u8 fat_init();
u8 fat_load_dir(u32 entry);
u8 fat_get_full_record(u32 lfn_entry, FatFullRecord *rec);
u8 fat_open_file(FatFullRecord *rec, u16 sectors);
u8 fat_skip_sectors(u16 secs);
u8 fat_read(u8 *dst, u16 secs, u8 memory);
void fat_make_path_name(u8 *dst, u8 *dir, u8 *filename);
void fat_make_sync_name(u8 * dir, u8 *filename, u8 *ext, u8 *dst);
u8 fat_open_file_by_name(u8 *name, FatFullRecord *rec, u16 sectors);
u8 fat_write_file(u8 *src, u16 secs);
u8 fat_make_dir(FatFullRecord *rec, u8 *name);
u32 fat_cluster_to_sector(u32 cluster);
void fat_bytes_to_int_le(u8 *src, u32 *dst);
void fat_int_to_bytes_le(u32 src, u8 *dst);

extern u32 fat_sub_dir_clust;
extern u16 fat_dir_size;
extern u32 *fat_dir;
extern File file;



//****************************************************************************** fat wr
u8 fat_seek_free_cluster(u32 *first_clst, u16 count);

#define FAT_DIRECTORY_ENTRY_SIZE 32
#define FAT_ENTRIES_PER_SECTOR 16
#define FAT_MAX_DIRECTORY_ENTRIES 1024
#define FAT_ENTRY_LOCATION_SHIFT 4
#define FAT_ENTRY_ATTRIBUTE_OFFSET 11
#define FAT_ATTRIBUTE_HIDDEN 0x02
#define FAT_ATTRIBUTE_VOLUME_LABEL 0x08
/* Hidden entries and volume labels are omitted from the browser. */
#define FAT_ATTRIBUTE_SKIP_BROWSER 0x0a
#define FAT_ATTRIBUTE_LONG_NAME 0x0f
#define FAT_ENTRY_DELETED 0xe5
#define FAT_LFN_LAST_ENTRY 0x40
#define FAT_LFN_SEQUENCE_MASK 0xf0
/* Cluster fields in the directory's second entry (".."). */
#define FAT_PARENT_CLUSTER_HIGH_OFFSET 0x34
#define FAT_PARENT_CLUSTER_LOW_OFFSET 0x3a

#endif	/* FAT__H */

