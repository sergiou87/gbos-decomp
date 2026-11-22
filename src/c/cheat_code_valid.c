typedef unsigned char u8;

extern u8 cheat_hex_digit(u8 character);

u8 cheat_code_valid(const u8 *code)
{
    u8 index;

    for (index = 0; index < 3; index++) {
        if (cheat_hex_digit(code[index]) > 15)
            return 0;
        if (cheat_hex_digit(code[index + 4]) > 15)
            return 0;
    }
    return 1;
}
