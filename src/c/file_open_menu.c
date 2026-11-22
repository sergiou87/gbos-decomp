typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"

typedef const u8 *Suffix;

extern u8 string_has_suffix(const u8 *extension, const u8 *name);
extern u8 string_has_any_suffix(const Suffix *extensions, const u8 *name);
extern u8 menu_select(const u8 **items, u16 selected);
extern u8 file_sram_menu(FatFile *file);
extern u8 file_text_menu(FatFile *file);
extern u8 load_game(FatFile *file);
extern const u8 file_menu_zip[];
extern const u8 file_menu_rar[];
extern const u8 file_menu_7z[];
extern const u8 file_menu_7z_space[];
extern const u8 file_menu_title[];
extern const u8 file_menu_unzip_warning[];
extern const u8 file_menu_sram_suffix[];
extern const u8 file_menu_text_suffix[];

u8 file_open_menu(FatFile *file)
{
    u8 result;
    volatile u8 error = 0;
    const u8 *message[3];
    const u8 *archives[5];

    archives[0] = file_menu_zip;
    archives[1] = file_menu_rar;
    archives[2] = file_menu_7z;
    archives[3] = file_menu_7z_space;
    archives[4] = 0;
    message[0] = file_menu_title;
    message[1] = file_menu_unzip_warning;
    message[2] = 0;

    result = string_has_suffix(file_menu_sram_suffix, file->name);
    if (result)
        return file_sram_menu(file);
    if (string_has_suffix(file_menu_text_suffix, file->name))
        return file_text_menu(file);
    if (string_has_any_suffix(archives, file->name)) {
        menu_select(message, 0);
        return 0;
    }
    if (!string_has_suffix(file_menu_sram_suffix, file->name))
        error = load_game(file);
    return error;
}

const unsigned char file_menu_zip[] = "zip";
const unsigned char file_menu_rar[] = "rar";
const unsigned char file_menu_7z[] = "7z";
const unsigned char file_menu_7z_space[] = "7z ";
const unsigned char file_menu_title[] = "File Menu";
const unsigned char file_menu_unzip_warning[] = "Unzip me!";
const unsigned char file_menu_sram_suffix[] = ".srm";
const unsigned char file_menu_text_suffix[] = ".txt";
