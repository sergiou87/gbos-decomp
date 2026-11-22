typedef unsigned char u8;
typedef unsigned int u16;
typedef struct FatFile FatFile;

extern u8 menu_select(const u8 **items, u16 selected);
extern u8 fat_open_file(FatFile *file, u16 mode);
extern u16 heap_alloc(u16 size);
extern u8 fat_read_file(u8 *buffer, u16 count, u8 mode);
extern void load_cheats(const u8 *data);
extern void heap_release(u16 size);

u8 file_text_menu(FatFile *file)
{
    u8 *buffer;
    u8 error;
    const u8 *items[] = {
        (const u8 *)"File Menu", (const u8 *)"Load cheats", (const u8 *)"Cancel", 0
    };

    if (menu_select(items, 0) == 0) {
        error = fat_open_file(file, 0);
        if (error)
            return error;
        buffer = (u8 *)heap_alloc(512);
        error = fat_read_file(buffer, 1, 1);
        if (!error)
            load_cheats(buffer);
        heap_release(512);
        return error;
    }
    return 0;
}
