typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"
#include "settings.h"

extern void *os_work_buffer;
extern FatFileState fat_file_state;
extern void clear_browser_screen(void);
extern void display_set_attributes(u16 enabled);
extern void display_print(const u8 *text);
extern void display_put_char(u8 character);
extern void draw_screen(void);
extern u16 heap_alloc(u16 size);
extern void heap_release(u16 size);
extern u8 backup_game_save(FatFile *file);
extern u8 load_game_save(FatFile *file);
extern u8 fat_open_file(FatFile *file, u16 mode);
extern u8 fat_read_file(u8 *buffer, u16 count, u8 mode);
extern void flash_erase(u32 address, u16 groups);
extern void select_mode1_bank(u16 bank);
extern void string_copy(const u8 *source, u8 *destination);
extern void settings_store(void);
extern void os_update_authentication(void);
extern const u8 program_game_warning[];
extern const u8 program_game_separator[];
extern const u8 program_game_erase_message[];
extern const u8 program_game_write_message[];

u8 program_game(FatFile *file)
{
    u8 error;
    u32 capacity;
    u8 groups;
    u8 index;
    u8 mapper;
    u8 ram_size;
    FatFile *temporary;
    u16 destination;
    u8 battery_types[] = {3, 6, 9, 0x0d, 0x0f, 0x10, 0x13, 0x17, 0x1b, 0x1e, 0xff, 0};
    u8 chunk;

    clear_browser_screen();
    display_set_attributes(0);
    display_print(program_game_warning);
    display_print(program_game_separator);
    draw_screen();
    temporary = (FatFile *)heap_alloc(sizeof(FatFile));
    error = backup_game_save(temporary);
    heap_release(sizeof(FatFile));
    if (error)
        return error;
    error = fat_open_file(file, 0);
    if (error)
        return error;
    if (fat_file_state.remaining > FLASH_MAX_SECTORS)
        fat_file_state.remaining = FLASH_MAX_SECTORS;
    groups = fat_file_state.remaining / FLASH_ERASE_SECTORS;
    if (fat_file_state.remaining % FLASH_ERASE_SECTORS)
        groups++;
    if (groups > FLASH_ERASE_MAX_GROUPS)
        groups = FLASH_ERASE_MAX_GROUPS;
    display_print(program_game_erase_message);
    draw_screen();
    flash_erase(0, groups);
    display_print(program_game_write_message);
    draw_screen();
    destination = 0;
    while (fat_file_state.remaining) {
        chunk = fat_file_state.remaining > FLASH_ERASE_SECTORS ?
                FLASH_ERASE_SECTORS : fat_file_state.remaining;
        error = fat_read_file((u8 *)&destination, chunk, 0);
        if (error)
            return error;
        display_put_char('.');
        draw_screen();
    }
    select_mode1_bank(0);
    mapper = *(u8 *)ROM_HEADER_MAPPER;
    ((Settings *)os_work_buffer)->mapper = MAPPER_MBC5;
    if (mapper == 0)
        ((Settings *)os_work_buffer)->mapper = MAPPER_ROM;
    if (mapper >= 1 && mapper <= 3)
        ((Settings *)os_work_buffer)->mapper = MAPPER_MBC1;
    if (mapper >= 5 && mapper <= 6)
        ((Settings *)os_work_buffer)->mapper = MAPPER_MBC2;
    if (mapper >= 8 && mapper <= 9)
        ((Settings *)os_work_buffer)->mapper = MAPPER_ROM;
    if (mapper >= 0x0f && mapper <= 0x13)
        ((Settings *)os_work_buffer)->mapper = MAPPER_MBC3;
    if (mapper >= 0x19 && mapper <= 0x1e)
        ((Settings *)os_work_buffer)->mapper = MAPPER_MBC5;
    capacity = ROM_SIZE_MIN_BYTES;
    ((Settings *)os_work_buffer)->rom_size = 0;
    while (capacity < file->size && capacity < ROM_SIZE_MAX_BYTES) {
        capacity <<= 1;
        ((Settings *)os_work_buffer)->rom_size++;
    }
    ram_size = *(u8 *)ROM_HEADER_RAM_SIZE;
    if ((ram_size == 1) | (ram_size == 2))
        ((Settings *)os_work_buffer)->ram_size = RAM_SIZE_8K;
    else
        ((Settings *)os_work_buffer)->ram_size = RAM_SIZE_32K;
    if (((Settings *)os_work_buffer)->mapper == MAPPER_MBC2)
        ((Settings *)os_work_buffer)->ram_size = RAM_SIZE_8K;
    index = 0;
    for (;;) {
        if (!battery_types[index]) {
            ((Settings *)os_work_buffer)->save_enabled = 0;
            break;
        }
        if (battery_types[index] == *(u8 *)0xa147) {
            ((Settings *)os_work_buffer)->save_enabled = 1;
            break;
        }
        index++;
    }
    string_copy(file->name, ((Settings *)os_work_buffer)->name);
    settings_store();
    temporary = (FatFile *)heap_alloc(sizeof(FatFile));
    error = load_game_save(temporary);
    heap_release(sizeof(FatFile));
    os_update_authentication();
    return error;
}
