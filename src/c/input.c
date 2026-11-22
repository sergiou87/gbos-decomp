typedef unsigned char u8;
typedef unsigned int u16;

#include "input_options.h"

extern u8 *os_work_buffer;
extern u8 key_repeat_counter;
extern u16 input_keys;
extern void poll_command_monitor(void);
extern void wait_vblank(void);
extern u8 read_joypad(void);
extern void input_key_event(u8 keys);

u16 poll_input(void);

void wait_keypress(void)
{
    while (poll_input())
        ;
    while (!poll_input())
        ;
}

u16 poll_input(void)
{
    u8 buttons;

    poll_command_monitor();
    if (key_repeat_counter < 22)
        wait_vblank();
    input_keys = read_joypad();
    if (((InputOptions *)os_work_buffer)->swap_buttons) {
        buttons = input_keys & 3;
        input_keys &= ~3;
        input_keys |= buttons >> 1;
        input_keys |= (buttons & 1) << 1;
    }
    if (input_keys && (input_keys & 0xf0))
        key_repeat_counter++;
    else
        key_repeat_counter = 0;
    if (key_repeat_counter > 24) {
        key_repeat_counter -= 2;
        input_keys = 0;
    }
    input_key_event(input_keys);
    return input_keys;
}
