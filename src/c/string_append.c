typedef unsigned char u8;
typedef unsigned int u16;

extern u8 string_length(const u8 *value);

void string_append(u8 *destination, const u8 *source)
{
    while (*destination)
        destination++;
    while (*source)
        *destination++ = *source++;
    *destination = 0;
}

void string_append_hex_byte(u8 value, u8 *text)
{
    u8 digit;

    text += string_length(text);
    text[2] = 0;
    digit = value >> 4;
    if (digit > 9)
        digit += 7;
    text[0] = digit + '0';
    digit = value & 0x0f;
    if (digit > 9)
        digit += 7;
    text[1] = digit + '0';
}

void string_append_hex_word(u16 value, u8 *text)
{
    string_append_hex_byte(value >> 8, text);
    string_append_hex_byte(value & 0xff, text);
}
