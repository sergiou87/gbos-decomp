typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u8 *fat_table_buffer;
extern u8 fat_read_entry(u32 cluster, u32 *value);
extern u8 fat_next_cluster(u32 *cluster);
extern void write_dword(u32 value, u8 *destination);
extern u8 fat_write_table_sector(void);

u8 fat_last_cluster(u32 cluster, u32 *last)
{
    u8 error;

    while (cluster != 0x0fffffff) {
        *last = cluster;
        error = fat_next_cluster(&cluster);
        if (error)
            return error;
    }
    return 0;
}

u8 fat_replace_last_link(u32 cluster, u32 next)
{
    u8 *entry;
    u8 error;
    u32 value;

    error = fat_read_entry(cluster, &value);
    if (error)
        return error;
    if (value != 0x0fffffff)
        return 0xfe;
    entry = fat_table_buffer + ((cluster & 0x7f) << 2);
    write_dword(next, entry);
    return fat_write_table_sector();
}
