typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"
#include "storage_layout.h"

extern u8 fat_open_file(FatFile *file, u16 mode);
extern void select_mode3_bank(u16 bank);
extern u8 fat_read_file(u8 *buffer, u16 count, u8 mode);
extern u8 fat_write_file(u16 buffer, u16 count);

u8 copy_file_to_sram(FatFile *file)
{
    u16 error;
    u16 count;
    u8 bank;
    u8 chunk;

    error = fat_open_file(file, 0);
    if (error)
        return error;
    count = file->size / STORAGE_SECTOR_SIZE;
    if (file->size % STORAGE_SECTOR_SIZE)
        count++;
    if (count > SAVE_RAM_SECTORS)
        count = SAVE_RAM_SECTORS;
    bank = 0;
    while (count) {
        select_mode3_bank(bank);
        chunk = count > WINDOW_SECTORS ? WINDOW_SECTORS : count;
        error = fat_read_file((u8 *)CARTRIDGE_WINDOW_ADDRESS, chunk, 1);
        count -= chunk;
        bank++;
        if (error)
            return error;
    }
    return 0;
}

u8 copy_sram_to_file(FatFile *file)
{
    u16 count;
    u16 error;
    u8 bank;
    u8 chunk;

    if (file->size < SAVE_RAM_SIZE) {
        count = file->size / STORAGE_SECTOR_SIZE;
        if (file->size % STORAGE_SECTOR_SIZE)
            count++;
    } else {
        count = SAVE_RAM_SECTORS;
    }
    error = fat_open_file(file, count);
    if (error)
        return error;
    bank = 0;
    while (count) {
        select_mode3_bank(bank);
        chunk = count > WINDOW_SECTORS ? WINDOW_SECTORS : count;
        error = fat_write_file(CARTRIDGE_WINDOW_ADDRESS, chunk);
        count -= chunk;
        bank++;
        if (error)
            return error;
    }
    return 0;
}
