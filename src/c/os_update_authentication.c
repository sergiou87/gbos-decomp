typedef unsigned char u8;
typedef unsigned int u16;

extern u16 input_header_marker;
extern void flash_restore_boot(void);
extern void os_authentication_challenge(void);

void os_update_authentication(void)
{
    u8 *data = (u8 *)0x01d0;
    u16 value;

    if (input_header_marker == 0x0e1a) {
        value = (data[0] ^ data[7]) | ((u16)(data[8] ^ data[15]) << 8);
        value = value + data[9] - data[14] -
                ((u16)data[16] << 8) + ((u16)data[23] << 8);
        data[32] = value & 0xff;
        data[33] = value >> 8;
        flash_restore_boot();
        os_authentication_challenge();
    }
}
