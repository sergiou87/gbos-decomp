typedef unsigned char u8;
typedef unsigned int u16;

extern u16 fat_sector_buffer;
extern const u8 fat_lfn_offsets[13];
extern void memory_fill(void *destination, u8 value, u16 count);

void fat_write_lfn_entry(const u8 *name, u8 slot, u8 part, u8 checksum)
{
    u8 *entry;
    u8 index;
    u8 offset;
    u8 padding;
    const u8 *source;

    padding = 0;
    entry = (u8 *)(fat_sector_buffer + slot * 32);
    source = name + part * 13;
    memory_fill(entry, 0, 32);
    /* Keep advancing the source after NUL; only the stored bytes are padded. */
    for (index = 0; index < 13; index++) {
        offset = fat_lfn_offsets[index];
        entry[offset] = *source++ | padding;
        entry[offset + 1] = padding;
        if (!entry[offset])
            padding = 0xff;
    }
    if (padding || !*source)
        padding = 0x40;
    entry[0] = (part + 1) | padding;
    entry[11] = 0x0f;
    entry[13] = checksum;
}
