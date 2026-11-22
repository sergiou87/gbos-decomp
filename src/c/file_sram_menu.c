typedef unsigned char u8;
typedef unsigned int u16;
typedef struct FatFile FatFile;

extern u8 menu_select(const u8 **items, u16 selected);
extern void clear_save_banks(void);
extern u8 copy_file_to_sram(FatFile *file);
extern u8 copy_sram_to_file(FatFile *file);

u8 file_sram_menu(FatFile *file)
{
    const u8 *items[] = {
        (const u8 *)"File Menu", (const u8 *)"Cancel",
        (const u8 *)"Copy File To SRAM", (const u8 *)"Copy SRAM To File", 0
    };
    u8 response = 0;

    response = menu_select(items, 0);
    if (response == 0)
        return 0;
    if (response == 1) {
        clear_save_banks();
        response = copy_file_to_sram(file);
        return response;
    }
    if (response == 2) {
        response = copy_sram_to_file(file);
        return response;
    }
    return 0;
}
