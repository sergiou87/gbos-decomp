typedef unsigned char u8;

u8 cheat_hex_digit(u8 character)
{
    if (character >= '0' && character <= '9')
        character -= '0';
    else if (character >= 'A' && character <= 'F')
        character -= 'A' - 10;
    else if (character >= 'a' && character <= 'f')
        character -= 'a' - 10;
    else
        return 0xff;
    return character & 0x0f;
}
