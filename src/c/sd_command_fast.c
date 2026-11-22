typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "sd_protocol.h"

extern void sd_select(void);
extern void sd_deselect(void);
extern void sd_send(u8 value);
extern u8 sd_exchange_nowait(u8 value);

u8 sd_command_fast(u8 command, u32 argument)
{
    u8 response;
    u16 timeout;

    sd_select();
    sd_send(SD_SPI_IDLE);
    sd_send(command);
    sd_send(argument >> 24);
    sd_send(argument >> 16);
    sd_send(argument >> 8);
    sd_send(argument);
    sd_send(SD_SPI_IDLE);
    sd_send(SD_SPI_IDLE);
    response = sd_exchange_nowait(SD_SPI_IDLE);
    timeout = 0;
    while (response == SD_SPI_IDLE) {
        response = sd_exchange_nowait(SD_SPI_IDLE);
        if (timeout++ == SD_RESPONSE_RETRIES)
            break;
    }
    sd_deselect();
    return response;
}
