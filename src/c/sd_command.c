typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u8 command_crc7(u8 *command);
extern void sd_select(void);
extern void sd_deselect(void);
extern u8 sd_exchange(u8 value);

u8 sd_command(u8 command, u32 argument) {

    u16 timeout = 0;
    u8 response;
    u8 packet[5];
    u8 crc;

    packet[0] = command;
    packet[1] = (argument >> 24) & 0xff;
    packet[2] = (argument >> 16) & 0xff;
    packet[3] = (argument >> 8) & 0xff;
    packet[4] = (argument >> 0) & 0xff;
    crc = command_crc7(packet) | 1;

    sd_select();
    sd_exchange(0xff);
    sd_exchange(command);
    sd_exchange(argument >> 24);
    sd_exchange(argument >> 16);
    sd_exchange(argument >> 8);
    sd_exchange(argument);
    sd_exchange(crc);
    sd_exchange(0xff);
    response = sd_exchange(0xff);

    while (response == 0xff) {
        response = sd_exchange(0xff);
        if (timeout++ == 2048)break;
    }

    sd_deselect();
    return response;
}
