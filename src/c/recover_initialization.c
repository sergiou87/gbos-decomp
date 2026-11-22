typedef unsigned char u8;
typedef unsigned int u16;

#include "settings.h"

extern u8 *os_work_buffer;
extern u8 cgb_detected;
extern void clear_browser_screen(void);
extern void draw_screen(void);
extern u8 sd_enter_idle(u8 force);
extern void set_register0(u8 value);
extern void set_register1(u8 value);
extern void normal_speed(void);
extern void launch_game(u16 mode);

u8 recover_initialization(void)
{
    Settings *options;
    u8 register0_mask;
    u8 register1_mask;

    clear_browser_screen();
    draw_screen();
    sd_enter_idle(0);
    options = (Settings *)os_work_buffer;
    register0_mask = options->rom_size > ROM_SIZE_MAX ? ROM_SIZE_MAX : options->rom_size;
    register1_mask = options->ram_size > RAM_SIZE_32K ? RAM_SIZE_32K : options->ram_size;
    /* r8604 emits byte shifts, including 255 shifts when ram_size is zero. */
    register0_mask = (1 << register0_mask) - 1;
    register1_mask = (1 << (register1_mask - 1)) - 1;
    set_register0(register0_mask);
    set_register1(register1_mask);
    if (cgb_detected)
        normal_speed();
    launch_game(((Settings *)os_work_buffer)->mapper);
    return 0;
}
