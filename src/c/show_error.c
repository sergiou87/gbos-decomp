typedef unsigned char u8;

extern void clear_browser_screen(void);
extern void display_center(const u8 *text, u8 y);
extern void display_move_cursor(signed char x, signed char y);
extern void display_hex_byte(u8 value);
extern void draw_screen(void);
extern void wait_keypress(void);

void show_error(u8 error)
{
    clear_browser_screen();
    display_center((const u8 *)"ERROR: 0x**", 5);
    display_move_cursor(-2, 0);
    display_hex_byte(error);
    draw_screen();
    wait_keypress();
}
