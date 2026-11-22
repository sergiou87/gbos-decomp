typedef unsigned char u8;
typedef unsigned int u16;

extern void display_set_cursor(u8 x, u8 y);
extern void display_write(const u8 *text);
extern u8 display_width(void);

void display_write_at(const u8 *text, u8 x, u8 y)
{
    display_set_cursor(x, y);
    display_write(text);
}

void display_center(const u8 *text, u8 y)
{
    u16 length = 0;

    while (text[length])
        length++;
    display_write_at(text, (display_width() - length) / 2, y);
}
