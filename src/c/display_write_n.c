typedef unsigned char u8;
typedef unsigned int u16;

extern u16 display_cursor;
extern u8 display_attributes;
extern u8 screen_buffer[360];

void display_write_n(const u8 *text, u16 limit)
{
    u16 remaining;
    u8 *destination;
    u8 length;
    const u8 *source;

    destination = screen_buffer + display_cursor;
    source = text;
    length = 0;
    remaining = limit;
    while (*source && remaining) {
        *destination++ = *source++ | display_attributes;
        length++;
        remaining--;
    }
    display_cursor += length;
}
