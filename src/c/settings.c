typedef unsigned char u8;
typedef unsigned int u16;

#include "settings.h"
#include "storage_layout.h"

extern u8 *os_work_buffer;
extern void select_mode3_bank(u16 bank);
extern void fill_window(u8 value);
extern void memory_copy(const u8 *source, u8 *destination, u16 count);
extern void memory_fill(void *destination, u8 value, u16 count);
extern void clear_browser_screen(void);
extern void display_set_attributes(u16 enabled);
extern void display_center(const u8 *text, u8 y);
extern void draw_screen(void);
extern void wait_keypress(void);

void settings_store(void)
{
    const u8 *source;

    select_mode3_bank(SETTINGS_BANK);
    source = os_work_buffer;
    memory_copy(source, (u8 *)CARTRIDGE_WINDOW_ADDRESS, sizeof(Settings));
}

void settings_initialize(void)
{
    u8 *destination;

    select_mode3_bank(SETTINGS_BANK);
    destination = os_work_buffer;
    memory_copy((const u8 *)CARTRIDGE_WINDOW_ADDRESS, destination, sizeof(Settings));
    if (((Settings *)os_work_buffer)->signature != SETTINGS_SIGNATURE) {
        memory_fill(os_work_buffer, 0, sizeof(Settings));
        ((Settings *)os_work_buffer)->mapper = MAPPER_MBC1;
        ((Settings *)os_work_buffer)->ram_size = RAM_SIZE_32K;
        ((Settings *)os_work_buffer)->rom_size = ROM_SIZE_MAX;
        ((Settings *)os_work_buffer)->signature = SETTINGS_SIGNATURE;
        ((Settings *)os_work_buffer)->swap_buttons = 0;
        settings_store();
        clear_browser_screen();
        display_set_attributes(0);
        display_center((const u8 *)"Settings", 7);
        display_center((const u8 *)"reset to default", 8);
        display_center((const u8 *)"Press any key", 10);
        draw_screen();
        wait_keypress();
        clear_browser_screen();
    }
    if (((Settings *)os_work_buffer)->swap_buttons > 1)
        ((Settings *)os_work_buffer)->swap_buttons = 0;
}

void clear_save_banks(void)
{
    u16 bank;

    for (bank = 0; bank < SAVE_RAM_BANKS; bank++) {
        select_mode3_bank(bank);
        fill_window(0xff);
    }
}
