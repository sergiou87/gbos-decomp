typedef unsigned char u8;

extern void clear_screen(void);
extern void display_set_cursor(u8 x, u8 y);

void clear_browser_screen(void)
{
    clear_screen();
    display_set_cursor(0, 0);
}
