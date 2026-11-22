/* Declarations from research/fat_arena_context.h, with its globals as externs. */


#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned long

typedef struct {
    u8 name[212];
    u32 first_entry;
    u32 entry;
    u32 cluster;
    u32 size;
    u8 is_directory;
} FatFile;

typedef struct {
    u32 cluster;
    u32 sector;
    u32 size;
    u16 sec_available;
    u8 writing;
    u8 cluster_offset;
} FatFileState;

u8 fat_initialize();
u8 fat_scan_directory(u32 entry);
u8 fat_load_metadata(u32 first_entry, FatFile *file);
u8 fat_open_file(FatFile *file, u16 writing);
u8 fat_advance_file(u16 slen);
u8 fat_read_file(u8 *dst, u16 slen, u8 memory);
void path_join(u8 *dst, u8 *directory, u8 *filename);
void build_save_path(u8 *directory, u8 *filename, u8 *ext, u8 *dst);
u8 fat_open_path(u8 *name, FatFile *file, u16 writing);
u8 fat_write_file(u8 *src, u16 slen);
u32 fat_cluster_to_sector(u32 cluster);
void read_dword(u8 *src, u32 *dst);
extern u32 fat_parent_cluster;
extern u16 fat_directory_count;
extern u32 *fat_work_buffer;
extern FatFileState fat_file;
u8 sd_read(u32 sd_addr, void *dst, u16 slen);
u8 sd_read_cartridge(u32 sd_addr, void *dst, u16 slen);
u8 bytes_equal(u8 *str1, u8 *str2, u8 len);
u8 string_length(const u8 *str);
void str_append(u8 *dst, u8 *src);
void *heap_alloc(u16 size);
u8 osStartGame();
u8 osMainMenu();
u8 osExitBrowser();
u32 fat_sector_to_cluster(u32 sector);
u8 fat_sector_in_cluster(u32 sector);
u8 fat_find_free_run(u32 *first, u16 count);
u8 fat_link_run(u32 first, u16 count);
u8 fat_zero_clusters(u32 first, u16 count);
void memory_fill(void *destination, u8 value, u16 count);
void memory_copy(const u8 *source, u8 *destination, u16 count);
u8 fat_make_short_name(const u8 *name, u8 *alias, u16 sequence);
u8 fat_advance_directory(u32 *cluster, u32 *sector, u8 *cluster_offset);
void fat_write_lfn_entry(const u8 *name, u8 slot, u8 part, u8 checksum);
u8 fat_write_cached_sector(void);
u8 fat_write_short_entry(const u8 *name, u8 slot, u8 attributes, u32 cluster, u32 size);



extern const u8 fat_lfn_offsets[];

extern u32 fat_entry_count;
extern u32 fat_table_start;
extern u32 fat_volume_start;
extern u32 fat_cached_sector;
extern u32 fat_cached_table_sector;
extern u32 fat_table_sectors;
extern u32 fat_root_sector;
extern u32 fat_data_start;
extern u16 fat_reserved_sectors;
extern u8 fat_sectors_per_cluster;
extern u8 fat_type;
extern u8 fat_cluster_shift;
extern u32 fat_cluster_cursor;
extern u32 fat_parent_cluster;
extern u16 fat_directory_count;
extern u8 fat_dir_is_root;
extern u8 *fat_sector_buffer;
extern u8 *fat_table_buffer;
extern u32 *fat_work_buffer;
extern FatFileState fat_file;

u8 fat_read_sector(u32 sector);
u8 fat_read_table_sector(u32 sector);
u8 fat_detect_type();
u16 fat_read_word(u8 *src);
u8 fat_next_cluster(u32 *cluster);
u32 fat_sector_to_cluster(u32 sector);
u8 fat_sector_in_cluster(u32 sector);
u8 fat_find_path(u8 *name, FatFile *file, u32 *parent, u8 directory);
u8 fat_create_path_entry(const u8 *name, u32 dir_entry, u16 slen, u8 is_directory);
u8 fat_extend_file(FatFile *file, u16 slen);
u8 fat_write_table();
u8 fat_catch_clusters(u32 base, u16 clen);
u8 fat_zero_clusters(u32 cluster, u16 clen);
u8 fat_link_run(u32 first, u16 count);
u8 fat_expand_chain(u32 last_clust, u16 csize, u8 clean);
u8 fat_chain_tail(u32 start_clust, u32 *end_clust);
u8 fat_read_entry(u32 cluster, u32 *val);


u8 fat_load_metadata(u32 location, FatFile *file)
{
    FatFile *descriptor;
    u8 slot;
    u32 sector;
    u32 cluster;
    u8 *part;
    u8 *entry;
    u8 error;
    u8 index;
    u8 *name;
    u8 sector_index;

    location |= 0;

    descriptor = file;
    name = (u8 *)(void *)descriptor;
    slot = location & 0x0f;
    sector = location >> 4;
    cluster = fat_sector_to_cluster(sector);
    sector_index = fat_sector_in_cluster(sector);
    entry = (u8 *)(fat_sector_buffer + slot * 32);
    name[0] = 0;
    name[208] = 0;
    file->first_entry = location;
    for (;;) {
        error = fat_read_sector(sector);
        if (error)
            return error;
        for (; slot < 16 && entry[11] == 0x0f; entry += 32, slot++) {
            const u8 *offsets;

            part = name + (((u16)entry[0] - 1) & 0x0f) * 13;
            if ((entry[0] & 0x40) == 0x40)
                part[13] = 0;
            offsets = fat_lfn_offsets;
            for (index = 0; index < 13; index++)
                *part++ = entry[*offsets++];
        }
        if (slot == 16) {
            slot = 0;
            entry = (u8 *)fat_sector_buffer;
            sector_index++;
            sector++;
            if (sector_index == fat_sectors_per_cluster) {
                error = fat_next_cluster(&cluster);
                if (error)
                    return error;
                sector = fat_cluster_to_sector(cluster);
                sector_index = 0;
            }
            continue;
        }
        break;
    }
    if (!name[0]) {
        for (index = 0; index < 11; index++)
            name[index] = entry[index];
        name[index] = 0;
    }
    file->entry = (sector << 4) | slot;
    file->is_directory = entry[11] & 0x10;
    file->cluster = fat_read_word(entry + 20);
    file->cluster <<= 16;
    file->cluster |= fat_read_word(entry + 26);
    read_dword(entry + 28, &file->size);
    return 0;
}
