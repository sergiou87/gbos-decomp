typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "sd_protocol.h"
#include "error.h"

extern u32 sd_next_sector;
extern u8 sd_card_type;
extern void sd_select(void);
extern void sd_deselect(void);
extern void sd_end_read(void);
extern u8 sd_command_fast(u8 command, u32 argument);
extern u8 sd_read_blocks(void *buffer, u16 count);
extern u8 sd_read_banks(void *cartridge_address, u16 count);

u8 sd_begin_read(u32 sector)
{
    if (!(sd_card_type & SD_CARD_HIGH_CAPACITY))
        sector <<= SD_SECTOR_ADDRESS_SHIFT;
    if (sd_command_fast(SD_CMD_READ_MULTIPLE_BLOCK, sector))
        return DISK_ERR_RD1;
    sd_select();
    return 0;
}

u8 sd_read(u32 sector, void *buffer, u16 count)
{
    u8 error = 0;
    if (sector != sd_next_sector) {
        sd_end_read();
        error = sd_begin_read(sector);
        if (error)
            return error;
        sd_next_sector = sector;
    }
    if (error)
        return error;
    sd_next_sector += count;
    error = sd_read_blocks(buffer, count);
    if (error)
        sd_deselect();
    return error;
}

u8 sd_read_cartridge(u32 sector, void *cartridge_address, u16 count)
{
    u8 error = 0;
    if (sector != sd_next_sector) {
        sd_end_read();
        error = sd_begin_read(sector);
        if (error)
            return error;
        sd_next_sector = sector;
    }
    if (error)
        return error;
    sd_next_sector += count;
    error = sd_read_banks(cartridge_address, count);
    if (error)
        sd_deselect();
    return error;
}
