/* Adapted from smspower/sms-everdrive-os b513cc8, FAT support headers.
 * Retained unused declarations preserve native SDCC allocation history.
 */

#ifndef GBOS_FAT_CONTEXT_H
#define GBOS_FAT_CONTEXT_H

#define u8      unsigned char
#define u16     unsigned short
#define u32     unsigned long

typedef struct {
    u8 name[212];
    u32 first_entry;
    u32 hdr_entry_8;
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
void build_save_path(u8 * directory, u8 *filename, u8 *ext, u8 *dst);
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
u8 string_length(u8 *str);
void str_append(u8 *dst, u8 *src);

void *heap_alloc(u16 size);
u8 osStartGame();
u8 osMainMenu();
u8 osExitBrowser();

#endif
