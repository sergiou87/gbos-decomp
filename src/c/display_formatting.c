typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern void display_write(const u8 *text);
extern void display_set_cursor(u8 x, u8 y);
extern void display_write_n(const u8 *text, u16 limit);
extern void display_decimal(u32 value);

void display_hex_byte(u8 value)
{
    u8 digit;
    u8 buffer[8];

    buffer[2] = 0;
    digit = value >> 4;
    if (digit > 9)
        digit += 7;
    buffer[0] = digit + '0';
    digit = value & 0x0f;
    if (digit > 9)
        digit += 7;
    buffer[1] = digit + '0';
    display_write(buffer);
}

void display_hex_word(u16 value)
{
    display_hex_byte(value >> 8);
    display_hex_byte(value & 0xff);
}

void display_hex_word_le(u16 value)
{
    display_hex_byte(value & 0xff);
    display_hex_byte(value >> 8);
}

void display_hex_dword(u32 value)
{
    display_hex_word(value >> 16);
    display_hex_word(value & 0xffff);
}

void display_write_at_n(const u8 *text, u8 x, u8 y, u16 limit)
{
    display_set_cursor(x, y);
    display_write_n(text, limit);
}

void display_decimal_at(u32 value, u8 x, u8 y)
{
    display_set_cursor(x, y);
    display_decimal(value);
}
