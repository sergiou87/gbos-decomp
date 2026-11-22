typedef unsigned char u8;
typedef unsigned int u16;

extern u16 input_keys;
extern u8 display_width(void);
extern u8 display_height(void);
extern void clear_browser_screen(void);
extern void display_set_attributes(u16 enabled);
extern void display_write_at_n(const u8 *text, u8 x, u8 y, u16 length);
extern void display_set_cursor(u8 x, u8 y);
extern void display_put_char(u8 character);
extern void draw_screen(void);
extern void wait_keypress(void);

void cheat_edit_code(u8 *code)
{
    u8 y;
    u8 x;
    u8 index;
    u8 character;

    x = (display_width() - 9) / 2;
    y = display_height() / 2;
    index = 0;
    clear_browser_screen();
    do {
        if (input_keys == 0x20)
            index = index == 0 ? 8 : index - 1;
        if (input_keys == 0x10)
            index = index == 8 ? 0 : index + 1;
        display_set_attributes(0);
        display_write_at_n(code, x, y, 9);
        display_set_attributes(1);
        display_set_cursor(x + index, y);
        display_put_char(code[index]);
        draw_screen();
        wait_keypress();
        character = code[index];
        if (input_keys == 0x40)
            code[index] = character == 'X' ? 'F' : character == '0' ? 'F' :
                          character == 'A' ? '9' : character - 1;
        if (input_keys == 0x80)
            code[index] = character == 'X' ? '0' : character == 'F' ? '0' :
                          character == '9' ? 'A' : character + 1;
    } while (input_keys != 1);
}
