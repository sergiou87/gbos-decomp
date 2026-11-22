typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u8 *fat_table_buffer;
extern u8 fat_read_table_sector(u32 sector);
extern u8 fat_write_table_sector(void);
extern void write_dword(u32 value, u8 *destination);

u8 fat_link_run(u32 cluster, u16 count)
{
    u8 error;
    u32 *entry;

    for (;;) {
        error = fat_read_table_sector(cluster >> 7);
        if (error)
            return error;
        entry = (u32 *)(fat_table_buffer + ((cluster & 0x7f) << 2));
        for (;;) {
            cluster++;
            count--;
            if (!count) {
                write_dword(0x0fffffff, (u8 *)entry);
                error = fat_write_table_sector();
                return error;
            }
            write_dword(cluster, (u8 *)entry);
            entry++;
            if (!(cluster & 0x7f))
                break;
        }
        error = fat_write_table_sector();
        if (error)
            return error;
    }
}
