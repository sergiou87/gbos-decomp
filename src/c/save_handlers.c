typedef unsigned char u8;
typedef unsigned int u16;
typedef struct FatFile FatFile;

#include "storage_layout.h"
#include "error.h"

typedef struct {
    u8 mapper_options[3];
    u8 save_enabled;
    u8 swap_buttons;
    u8 name[208];
} SaveOptions;

extern u8 *os_work_buffer;
extern void clear_save_banks(void);
extern void select_mode3_bank(u16 bank);
extern u8 check_window(u8 value);
extern void build_save_path(const u8 *directory, const u8 *name,
                            const u8 *extension, u8 *destination);
extern u8 fat_open_path(const u8 *path, FatFile *file, u16 mode);
extern u8 fat_make_directory(FatFile *file, const u8 *path);
extern void display_print(const u8 *text);
extern void draw_screen(void);
extern u8 copy_file_to_sram(FatFile *file);
extern u8 copy_sram_to_file(FatFile *file);

u8 load_game_save(FatFile *file)
{
    u8 path[256];
    u16 error;

    if (!((SaveOptions *)os_work_buffer)->save_enabled)
        return 0;
    clear_save_banks();
    build_save_path((const u8 *)"/EDGB/SAVE", ((SaveOptions *)os_work_buffer)->name,
                    (const u8 *)"srm", path);
    error = fat_open_path(path, file, 0);
    if (error == FAT_ERR_NOT_EXIST)
        return 0;
    if (error)
        return error;
    display_print((const u8 *)"Load save RAM...");
    draw_screen();
    return copy_file_to_sram(file);
}

u8 backup_game_save(FatFile *file)
{
    u16 error;
    u8 path[256];
    u16 bank;

    if (!((SaveOptions *)os_work_buffer)->save_enabled || !((SaveOptions *)os_work_buffer)->name[0])
        return 0;
    for (bank = 0; bank < SAVE_RAM_BANKS; bank++) {
        select_mode3_bank(bank);
        error = check_window(0xff);
        if (!error)
            break;
    }
    if (bank == SAVE_RAM_BANKS)
        return 0;
    display_print((const u8 *)"Backup save RAM...");
    draw_screen();
    build_save_path((const u8 *)"/EDGB/SAVE", ((SaveOptions *)os_work_buffer)->name,
                    (const u8 *)"srm", path);
    error = fat_open_path(path, file, SAVE_RAM_SECTORS);
    if (error == FAT_ERR_PATH_NOT_EXIST) {
        error = fat_make_directory(file, (const u8 *)"/EDGB");
        /* The original uses bitwise AND, so most even errors also continue. */
        if (error & (error != FAT_ERR_EXIST))
            return error;
        error = fat_make_directory(file, (const u8 *)"/EDGB/SAVE");
        if (error)
            return error;
        error = fat_open_path(path, file, SAVE_RAM_SECTORS);
    }
    if (error)
        return error;
    return copy_sram_to_file(file);
}
