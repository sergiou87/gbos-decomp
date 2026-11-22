typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "fat_file.h"

extern u16 input_keys;
extern u16 fat_directory_count;
extern u32 *fat_work_buffer;
extern u32 fat_parent_cluster;
extern FatFile browser_file;
extern u16 browser_history[32];
extern u8 display_page_rows(void);
extern u8 fat_scan_directory(u32 cluster);
extern u8 browser_render(u8 redraw, u16 selected);
extern void wait_keypress(void);
extern u8 fat_load_metadata(u32 location, FatFile *file);
extern u8 file_open_menu(FatFile *file);
extern u8 browser_exit(void);
extern u8 main_menu(void);
extern u8 recover_initialization(void);

u8 file_browser(void)
{
    u32 location;
    u32 cluster;
    u16 depth;
    u8 error;
    u16 selected;
    u16 offset;
    u8 rows;
    u16 first;
    u8 redraw;

    selected = 0;
    rows = display_page_rows();
    redraw = 2;
    cluster = 0;
    depth = 0;
    for (;;) {
        if (redraw == 2) {
            error = fat_scan_directory(cluster);
            if (error)
                return error;
        }
        error = browser_render(redraw, selected);
        if (error)
            return error;
        redraw = 0;
        wait_keypress();
        if ((input_keys & 0x40) == 0x40) {
            first = (selected / rows) * rows;
            offset = selected - first;
            offset = offset == 0 ? rows - 1 : offset - 1;
            selected = first + offset;
            if (selected >= fat_directory_count && fat_directory_count)
                selected = fat_directory_count - 1;
            continue;
        }
        if ((input_keys & 0x80) == 0x80) {
            first = (selected / rows) * rows;
            offset = selected - first;
            offset++;
            if (offset >= rows)
                offset = 0;
            selected = first + offset;
            if (selected >= fat_directory_count)
                selected = first;
            continue;
        }
        if ((input_keys & 0x10) == 0x10) {
            if (rows < fat_directory_count) {
                first = (selected / rows) * rows;
                offset = selected - first;
                first += rows;
                if (first >= fat_directory_count)
                    first = 0;
                selected = first + offset;
                if (selected >= fat_directory_count)
                    selected = fat_directory_count - 1;
                redraw = 1;
            } else {
                continue;
            }
        }
        if ((input_keys & 0x20) == 0x20) {
            if (rows < fat_directory_count) {
                first = (selected / rows) * rows;
                offset = selected - first;
                if (selected < rows) {
                    selected = (fat_directory_count / rows) * rows + offset;
                    if (selected >= fat_directory_count)
                        selected = fat_directory_count - 1;
                } else {
                    selected -= rows;
                }
                redraw = 1;
            } else {
                continue;
            }
        }
        if ((input_keys & 2) == 2 && fat_directory_count) {
            error = fat_load_metadata((location = fat_work_buffer[selected], location), &browser_file);
            if (error)
                return error;
            if (browser_file.is_directory) {
                if (depth >= 32)
                    continue;
                cluster = browser_file.cluster;
                redraw = 2;
                browser_history[depth++] = selected;
                selected = 0;
                continue;
            }
            redraw = 2;
            error = file_open_menu(&browser_file);
            if (error)
                return error;
            continue;
        }
        if ((input_keys & 1) == 1 && depth == 0) {
            redraw = 2;
            error = browser_exit();
            if (error)
                return error;
        }
        if ((input_keys & 1) == 1 && depth) {
            cluster = fat_parent_cluster;
            redraw = 2;
            selected = browser_history[--depth];
            continue;
        }
        if ((input_keys & 4) == 4) {
            error = main_menu();
            if (error)
                return error;
            redraw = 2;
            continue;
        }
        if ((input_keys & 8) == 8) {
            error = recover_initialization();
            return error;
        }
    }
}
