typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u8 fat_sectors_per_cluster;
extern u8 *fat_sector_buffer;
extern u16 fat_directory_count;
extern u8 string_length(const u8 *text);
extern void memory_fill(void *destination, u8 value, u16 count);
extern void memory_copy(const u8 *source, u8 *destination, u16 count);
extern u8 fat_find_free_run(u32 *first, u16 count);
extern u8 fat_link_run(u32 first, u16 count);
extern u8 fat_zero_clusters(u32 first, u16 count);
extern u8 fat_make_short_name(const u8 *name, u8 *alias, u16 sequence);
extern u8 fat_advance_directory(u32 *cluster, u32 *sector, u8 *cluster_offset);
extern void fat_write_lfn_entry(const u8 *name, u8 slot, u8 part, u8 checksum);
extern u8 fat_write_cached_sector(void);
extern u8 fat_write_short_entry(const u8 *name, u8 slot, u8 attributes, u32 cluster, u32 size);

u8 fat_create_path_entry(const u8 *path, u32 parent, u16 sectors, u8 directory)
{
    u8 length;
    u8 padded[12];
    u8 parts;
    u8 cluster_offset;
    u8 alias[11];
    u32 first;
    u16 clusters;
    u8 *last_entry;
    u8 checksum = 0;
    u32 sector;
    const u8 *name;
    u8 index;
    u8 error;
    u8 attributes;
    u16 slot;

    name = path;
    while (*path) {
        if (*path == '/')
            name = path;
        path++;
    }
    if (*name == '/')
        name++;
    if (parent < 2)
        parent = 2;
    length = string_length(name);
    if (length < 11) {
        memory_fill(padded, ' ', 11);
        padded[11] = 0;
        for (index = 0; name[index]; index++)
            padded[index] = name[index];
        name = padded;
        length = 11;
    }
    first = 0;
    clusters = sectors / fat_sectors_per_cluster;
    if (sectors % fat_sectors_per_cluster)
        clusters++;
    error = fat_find_free_run(&first, clusters);
    if (error)
        return error;
    error = fat_link_run(first, clusters);
    if (error)
        return error;
    if (directory) {
        error = fat_zero_clusters(first, 1);
        if (error)
            return error;
    }
    parts = 0;
    if (length == 11)
        memory_copy(name, alias, 11);
    if (length > 11) {
        parts = length / 13;
        if (length % 13)
            parts++;
        checksum = fat_make_short_name(name, alias, fat_directory_count);
    }
    attributes = directory ? 0x10 : 0x20;
    sector = 0;
    last_entry = (u8 *)fat_sector_buffer + 480;
    for (;;) {
        error = fat_advance_directory(&parent, &sector, &cluster_offset);
        if (error)
            return error;
        if (*last_entry)
            continue;
        for (slot = 0; slot < 512; slot += 32)
            if (!*((u8 *)fat_sector_buffer + slot))
                break;
        if (slot != 512)
            break;
    }
    slot >>= 5;
    for (index = 0; index < parts; index++) {
        fat_write_lfn_entry(name, slot, parts - index - 1, checksum);
        slot++;
        if (slot == 16) {
            slot = 0;
            error = fat_write_cached_sector();
            if (error)
                return error;
            error = fat_advance_directory(&parent, &sector, &cluster_offset);
            if (error)
                return error;
        }
    }
    if (directory)
        sectors = 0;
    error = fat_write_short_entry(alias, slot, attributes, first, (u32)sectors << 9);
    return error;
}
