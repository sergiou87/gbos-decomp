#include "types.h"
#include "os.h"
#include "fat.h"
#include "sys.h"

extern Options *os_work_buffer;
extern u16 input_keys;
extern void clear_browser_screen(void);
extern void display_set_attributes(u16 enabled);
extern void display_fill(u16 tile, u8 x, u8 y, u8 width, u8 height);
extern void display_center(const u8 *text, u8 y);
extern void display_set_cursor(u8 x, u8 y);
extern void display_put_char(u8 character);
extern void display_write_at(const u8 *text, u8 x, u8 y);
extern void draw_screen(void);
extern void wait_keypress(void);
extern void settings_store(void);

void settings_menu(void)
{
    u16 x = 2;
    u16 y = 2;
    u16 width = 15;
    u16 count = 5;
    u8 row;
    u8 counts[5];
    const u8 **values;
    u16 selected;
    u16 index;
    const u8 *labels[5];
    u8 *options;
    const u8 *mappers[6];
    const u8 *rom_sizes[8];
    const u8 *ram_sizes[4];
    const u8 *toggles[2];

    selected = 0;
    options = (u8 *)os_work_buffer;
    labels[0] = "Mapper  :";
    labels[1] = "ROM Size:";
    labels[2] = "RAM Size:";
    labels[3] = "Save RAM:";
    labels[4] = "Swap Key:";
    mappers[0] = "????";
    mappers[1] = "MBC1";
    mappers[2] = "MBC2";
    mappers[3] = "MBC3";
    mappers[4] = "ROM ";
    mappers[5] = "MBC5";
    rom_sizes[0] = "64K ";
    rom_sizes[1] = "128K";
    rom_sizes[2] = "256K";
    rom_sizes[3] = "512K";
    rom_sizes[4] = "1M  ";
    rom_sizes[5] = "2M  ";
    rom_sizes[6] = "4M  ";
    rom_sizes[7] = "8M  ";
    ram_sizes[0] = "0  ";
    ram_sizes[1] = "8K ";
    ram_sizes[2] = "16K";
    ram_sizes[3] = "32K";
    toggles[0] = "NO ";
    toggles[1] = "YES";
    counts[0] = 6;
    counts[1] = 8;
    counts[2] = 4;
    counts[3] = 2;
    counts[4] = 2;

    clear_browser_screen();
    for (;;) {
        display_set_attributes(1);
        display_fill(' ', x, y, width, count * 2 + 3);
        display_center("Options", y);
        display_set_cursor(x, y + 1);
        for (index = 0; index < width; index++)
            display_put_char('-');
        for (index = 0; index < count; index++) {
            if (selected == index)
                display_set_attributes(0);
            else
                display_set_attributes(1);
            values = labels;
            row = y + index * 2 + 3;
            display_write_at(values[index], x + 1, row);
            values = index == 0 ? mappers :
                     index == 1 ? rom_sizes :
                     index == 2 ? ram_sizes : toggles;
            display_write_at(values[options[index]], x + 10, row);
        }
        draw_screen();
        wait_keypress();
        if (input_keys & 1) {
            settings_store();
            return;
        }
        if (input_keys & 0x40) {
            selected = selected == 0 ? count - 1 : selected - 1;
        } else if (input_keys & 0x80) {
            selected = selected == count - 1 ? 0 : selected + 1;
        } else if (input_keys & 2) {
            ++options[selected];
            if (options[selected] >= counts[selected])
                options[selected] = 0;
            if (!os_work_buffer->mapper)
                os_work_buffer->mapper = 1;
        }
    }
}
