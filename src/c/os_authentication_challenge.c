typedef unsigned char u8;
typedef unsigned int u16;

extern u16 input_keys;
extern void display_print(const u8 *text);
extern void display_hex_byte(u8 value);
extern void draw_screen(void);
extern void wait_keypress(void);
extern void flash_patch_boot(void);

void os_authentication_challenge(void)
{
    u8 index;
    const u16 *words = (const u16 *)0x01d0;
    u16 response;

    for (index = 0; index < 16; index++) {
        if (!(index & 3))
            display_print((const u8 *)"");
        display_hex_byte(*words);
        display_hex_byte(*words++ >> 8);
    }
    draw_screen();
    response = 0;
    for (index = 0; index < 16; index++) {
        wait_keypress();
        response <<= 1;
        if (input_keys == 0x40)
            response |= 1;
    }
    response ^= 0xa5ed;
    if (response == *words)
        flash_patch_boot();
    else
        for (;;)
            wait_keypress();
}
