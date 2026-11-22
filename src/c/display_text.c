typedef unsigned char u8;
typedef unsigned int u16;

extern u16 display_cursor;
extern u8 display_attributes;
extern u8 screen_buffer[360];

void display_set_cursor(u8 x, u8 y);

void display_fill(u16 tile, u8 x, u8 y, u8 width, u8 height)
{
    u8 column;
    u8 value = tile | display_attributes;
    u8 rows;
    u8 *destination;

    display_set_cursor(x, y);
    rows = height;
    while (rows--) {
        destination = screen_buffer + display_cursor;
        for (column = 0; column < width; column++)
            *destination++ = value;
        display_cursor += 20;
    }
}

void display_write(const u8 *text)
{
    const u8 *source = text;

    while (*source)
        screen_buffer[display_cursor++] = *source++ | display_attributes;
}

void display_print(const u8 *text)
{
    display_cursor = (display_cursor / 20) * 20 + 20;
    display_write(text);
}

void display_set_cursor(u8 x, u8 y)
{
    display_cursor = x + y * 20;
}

void display_move_cursor(signed char x, signed char y)
{
    display_cursor += x;
    display_cursor += y * 20;
}

void display_put_char(u8 value)
{
    screen_buffer[display_cursor++] = value | display_attributes;
}
