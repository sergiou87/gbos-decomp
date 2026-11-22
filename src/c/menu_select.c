#include "types.h"
#include "fat.h"


#include "os.h"
#include "sys.h"
#include "str.h"
#include "disk.h"
#include "bios.h"
#include "error.h"


extern u16 input_keys;
extern u8 string_length(const u8 *value);
extern u8 display_width(void);
extern u8 display_height(void);
extern void display_frame(u16 x, u16 y, u16 width, u16 height);
extern void display_set_attributes(u16 enabled);
extern void display_center(const u8 *text, u8 y);
extern void draw_screen(void);
extern void wait_keypress(void);

u8 menu_select(const u8 **items, u16 initial_selection)
{
    u16 length;
    u16 index;
    u16 x;
    u16 width;
    u16 selected = initial_selection;
    u16 count = 0;
    u16 y;

    while (items[count])
        count++;
    count--;
    width = 0;
    for (index = 0; index < count + 1; index++) {
        length = string_length(items[index]);
        if (width < length)
            width = length;
    }
    length = count * 2 + 1;
    x = (display_width() - width) / 2;
    y = (display_height() - length) / 2 - 1;
    display_frame(x, y, width, length);
    display_set_attributes(1);
    display_center(items[0], y - 1);
    for (;;) {
        for (index = 0; index < count; index++) {
            if (index == selected)
                display_set_attributes(0);
            else
                display_set_attributes(1);
            display_center(items[index + 1], index * 2 + 2 + y);
        }
        draw_screen();
        wait_keypress();
        if (input_keys & 0x40)
            selected = selected == 0 ? count - 1 : selected - 1;
        if (input_keys & 0x80)
            selected = selected == count - 1 ? 0 : selected + 1;
        if (input_keys & 1)
            return 0xff;
        if (input_keys & 2)
            return selected;
    }
}
