/* Adapted from smspower/sms-everdrive-os b513cc8, src/fat.c.
 * Only the inserted recovered function is linked; preceding code preserves
 * native SDCC allocation history and is not additional ROM ownership.
 */

#include "fat_context.h"

static const u8 lfn_char_struct[] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

u32 fat_entry_count;
u32 fat_table_start;
u32 fat_volume_start;
u32 fat_cached_sector;
u32 fat_cached_table_sector;
u32 fat_table_sectors;
u32 fat_root_sector;
u32 fat_data_start;
u16 fat_reserved_sectors;
u8 fat_sectors_per_cluster;
u8 fat_type;
u8 fat_cluster_shift;

u32 fat_cluster_cursor;
u32 fat_parent_cluster;
u16 fat_directory_count;
u8 fat_dir_is_root;

u8 *fat_sector_buffer;
u8 *fat_table_buffer;

u32 *fat_work_buffer;
FatFileState fat_file;

u8 fat_read_sector(u32 sector);
u8 fat_read_table_sector(u32 sector);
u8 fat_detect_type();

u16 fat_read_word(u8 *src);
u8 fat_next_cluster(u32 *cluster);

u32 fat_sector_to_cluster(u32 sector);
u8 fat_sector_in_cluster(u32 sector);
u8 fat_find_path(u8 *name, FatFile *file, u32 *parent, u8 directory);

u8 fat_create_path_entry(u8 *name, u32 dir_entry, u16 slen, u8 is_directory);
u8 fat_extend_file(FatFile *file, u16 slen);
u8 fat_write_table();
u8 fat_catch_clusters(u32 base, u16 clen);
u8 fat_zero_clusters(u32 cluster, u16 clen);
u8 fat_link_run(u32 last_clust, u32 next_clust);
u8 fat_expand_chain(u32 last_clust, u16 csize, u8 clean);
u8 fat_chain_tail(u32 start_clust, u32 *end_clust);
u8 fat_read_entry(u32 cluster, u32 *val);

void path_join(u8 *dst, u8 *directory, u8 *filename)
{
    dst[0] = 0;
    str_append(dst, directory);
    if (string_length(directory) != 1)
        str_append(dst, "/");
    str_append(dst, filename);
}

/* Replaced with the recovered definition before compiling this unit. */
u8 fat_find_path(u8 *name, FatFile *file, u32 *parent, u8 directory) {}
