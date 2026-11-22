typedef unsigned char u8;
typedef unsigned int u16;

extern u8 cheat_hex_digit(u8 character);

u16 cheat_code_address(const u8 *code)
{
    u16 address;

    address = cheat_hex_digit(code[5]);
    if (address < 8)
        return 0xffff;
    address = 15 - address;
    address <<= 12;
    address |= (u16)cheat_hex_digit(code[2]) << 8;
    address |= (u16)cheat_hex_digit(code[3]) << 4;
    address |= cheat_hex_digit(code[4]);
    return address;
}
