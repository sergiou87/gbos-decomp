typedef unsigned char u8;
typedef unsigned int u16;

#include "input_options.h"

extern u16 input_header_marker;
extern u16 input_keys;
extern u8 *os_work_buffer;
extern const u8 authentication_message[];
extern void clear_browser_screen(void);
extern void display_print(const u8 *text);
extern void draw_screen(void);
extern void wait_keypress(void);
extern void os_authentication_challenge(void);

void prepare_os_reload(void)
{
    u8 sequence[] = {1, 2, 0x40, 2, 0x40, 2, 1, 0x80};
    u8 index;

    if (input_header_marker == 0xffff) {
        clear_browser_screen();
        display_print(authentication_message);
        ((InputOptions *)os_work_buffer)->swap_buttons = 0;
        draw_screen();
        index = 0;
        do {
            wait_keypress();
            if (input_keys == sequence[index]) {
                index++;
            } else {
                index = 0;
                if (input_keys == sequence[0])
                    index = 1;
            }
        } while (index < 8);
        os_authentication_challenge();
    }
}
