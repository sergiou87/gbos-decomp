typedef unsigned char u8;
typedef unsigned int u16;

u8 command_crc7(u8 *command)
{
    u8 length = 5;
    u16 crc = 0;
    u16 bit;

    while (length--) {
        crc ^= *command++;
        bit = 8;
        do {
            crc <<= 1;
            if (crc & 0x100)
                crc ^= 0x12;
        } while (--bit);
    }
    return crc & 0xfe;
}
