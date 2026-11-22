typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"

extern u16 fat_directory_count;
extern u32 *fat_work_buffer;
extern u16 browser_previous_selection;
extern u8 display_width(void);
extern u16 heap_alloc(u16 size);
extern void heap_release(u16 size);
extern void clear_browser_screen(void);
extern void display_set_attributes(u16 enabled);
extern void display_fill(u16 tile, u8 x, u8 y, u8 width, u8 height);
extern void display_write_at(const u8 *text, u8 x, u8 y);
extern void display_write_at_n(const u8 *text, u8 x, u8 y, u16 limit);
extern void display_write(const u8 *text);
extern void display_decimal(u32 value);
extern void display_center(const u8 *text, u8 y);
extern u8 string_length(const u8 *text);
extern u8 fat_load_metadata(u32 location, FatFile *file);
extern void draw_screen(void);
extern const u8 browser_page_prefix[];
extern const u8 browser_page_separator[];
extern const u8 browser_empty_message[];

u8 browser_render(u8 redraw, u16 selected)
{
    u32 location;
    u8 chosen;
    FatFile *file;
    u16 first = (selected / 12) * 12;
    u8 error;
    u16 index;
    u8 row = 0;
    u8 width = display_width();
    u16 pages;

    file = (FatFile *)heap_alloc(sizeof(FatFile));
    if (redraw)
        clear_browser_screen();
    display_set_attributes(0);
    if (redraw) {
        browser_previous_selection = 0;
        display_fill(0, 0, 0, width, 1);
    }
    display_fill(0, 0, 15, width, 2);
    if (fat_directory_count) {
        display_write_at(browser_page_prefix, 0, 0);
        display_decimal(first / 12 + 1);
        display_write(browser_page_separator);
        pages = fat_directory_count / 12;
        if (fat_directory_count % 12)
            pages++;
        display_decimal(pages);
        row = 2;
    }
    error = 0;
    if (!fat_directory_count)
        display_center(browser_empty_message, 8);
    for (index = first;
         index < (u16)(first + 12) && index < fat_directory_count;
         index++, row++) {
        display_set_attributes(0);
        chosen = selected == index;
        if (chosen || browser_previous_selection == index || redraw) {
            location = fat_work_buffer[index];
            error = fat_load_metadata(location, file);
            if (error)
                break;
            if (chosen) {
                display_set_attributes(1);
                display_fill(' ', 0, 15, 20, 3);
                display_write_at_n(file->name, 0, 15, 20);
                if (string_length(file->name) > 20) {
                    display_write_at_n(file->name + 20, 0, 16, 20);
                    if (string_length(file->name + 20) > 20)
                        display_write_at_n(file->name + 40, 0, 17, 20);
                }
            }
            if (!redraw || chosen)
                display_fill(' ', 0, row, 20, 1);
            display_write_at_n(file->name, 0, row, 20);
        }
    }
    heap_release(sizeof(FatFile));
    browser_previous_selection = selected;
    draw_screen();
    return error;
}
