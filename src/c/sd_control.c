typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "sd_protocol.h"
#include "error.h"

__at(0xc001) u32 sd_next_sector;
__at(0x0203) u8 sd_card_type;
__at(0xc005) u8 sd_initialized;

extern void sd_select(void);
extern void sd_deselect(void);
extern u8 sd_command(u8 command, u32 argument);
extern u8 sd_command_fast(u8 command, u32 argument);
extern u8 sd_write_blocks(void *buffer, u16 count);

void sd_end_read(void)
{
    if (sd_next_sector == SD_NO_READ_STREAM)
        return;
    sd_deselect();
    sd_command_fast(SD_CMD_STOP_TRANSMISSION, 0);
    sd_next_sector = SD_NO_READ_STREAM;
}

u8 sd_begin_write(u32 sector)
{
    if (!(sd_card_type & SD_CARD_HIGH_CAPACITY))
        sector <<= SD_SECTOR_ADDRESS_SHIFT;
    if (sd_command_fast(SD_CMD_WRITE_MULTIPLE_BLOCK, sector))
        return DISK_ERR_WR1;
    sd_select();
    return 0;
}

u8 sd_write(u32 sector, void *buffer, u16 count)
{
    u8 error;
    sd_end_read();
    error = sd_begin_write(sector);
    if (error)
        return error;
    error = sd_write_blocks(buffer, count);
    sd_deselect();
    if (error)
        return error;
    sd_end_read();
    return 0;
}

u8 sd_enter_idle(u8 force)
{
    u8 response;
    u8 retries;

    if (!force && !sd_initialized)
        return 0;
    response = 0;
    retries = 0;
    while (response != SD_RESPONSE_IDLE && retries++ < 2)
        response = sd_command(SD_CMD_GO_IDLE, 0);
    if (response != SD_RESPONSE_IDLE)
        return DISK_ERR_INIT;
    return 0;
}
