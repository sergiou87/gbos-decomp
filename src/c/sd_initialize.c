typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#include "sd_protocol.h"

extern u32 sd_next_sector;
extern u8 sd_card_type;
extern u8 sd_initialized;
extern void sd_enable_fast(void);
extern void sd_deselect(void);
extern void sd_select(void);
extern void sd_end_read(void);
extern u8 sd_exchange(u8 value);
extern u8 sd_enter_idle(u8 force);
extern u8 sd_command(u8 command, u32 argument);

u8 sd_initialize(void)
{
    u8 response;
    u16 i;

    if (sd_card_type != SD_CARD_UNINITIALIZED) {
        sd_enable_fast();
        sd_next_sector = 1;
        sd_initialized = 1;
        sd_end_read();
        return 0;
    }
    sd_initialized = 0;
    sd_card_type = 0;
    sd_deselect();
    sd_end_read();
    for (i = 0; i < 32; i++)
        sd_exchange(SD_SPI_IDLE);
    response = sd_enter_idle(1);
    if (response)
        return response;
    response = sd_command(SD_CMD_SEND_IF_COND, SD_IF_COND_ARGUMENT);
    for (i = 0; i < 5; i++)
        sd_exchange(SD_SPI_IDLE);
    if (response == SD_SPI_IDLE)
        return SD_ERR_IF_COND_TIMEOUT;
    if (response != SD_RESPONSE_ILLEGAL_IDLE)
        sd_card_type |= SD_CARD_VERSION_2;

    if (sd_card_type == SD_CARD_VERSION_2) {
        for (i = 0; i < SD_RESPONSE_RETRIES; i++) {
            response = sd_command(SD_CMD_APP_COMMAND, 0xffff);
            if (response == SD_SPI_IDLE)
                return SD_ERR_V2_APP_COMMAND_TIMEOUT;
            if (response == SD_RESPONSE_IDLE) {
                response = sd_command(SD_ACMD_SEND_OP_COND, SD_HIGH_CAPACITY_ARGUMENT);
                if (response == SD_SPI_IDLE)
                    return SD_ERR_V2_OP_COND_TIMEOUT;
                if (!response)
                    break;
            }
        }
        if (i == SD_RESPONSE_RETRIES)
            return SD_ERR_V2_INIT_TIMEOUT;
        response = sd_command(SD_CMD_READ_OCR, 0);
        if (response == SD_SPI_IDLE)
            return SD_ERR_OCR_TIMEOUT;
        sd_select();
        response = sd_exchange(SD_SPI_IDLE);
        for (i = 0; i < 3; i++)
            sd_exchange(SD_SPI_IDLE);
        if (response & SD_OCR_HIGH_CAPACITY)
            sd_card_type |= SD_CARD_HIGH_CAPACITY;
    } else {
        response = sd_command(SD_CMD_APP_COMMAND, 0);
        if (response == SD_SPI_IDLE)
            return SD_ERR_LEGACY_APP_COMMAND_TIMEOUT;
        response = sd_command(SD_ACMD_SEND_OP_COND, 0);
        if (response == SD_SPI_IDLE)
            return SD_ERR_LEGACY_OP_COND_TIMEOUT;
        for (i = 0; i < SD_RESPONSE_RETRIES; i++) {
            if (response < 1) {
                response = sd_command(SD_CMD_APP_COMMAND, 0);
                if (response == SD_SPI_IDLE)
                    return SD_ERR_LEGACY_RETRY_APP_TIMEOUT;
                if (response == SD_RESPONSE_IDLE) {
                    response = sd_command(SD_ACMD_SEND_OP_COND, 0);
                    if (response == SD_SPI_IDLE)
                        return SD_ERR_LEGACY_RETRY_OP_TIMEOUT;
                    if (!response)
                        break;
                }
            } else {
                response = sd_command(SD_CMD_SEND_OP_COND, 0);
                if (!response)
                    break;
            }
        }
        if (i == SD_RESPONSE_RETRIES)
            return SD_ERR_LEGACY_INIT_TIMEOUT;
    }
    sd_enable_fast();
    sd_next_sector = 1;
    sd_initialized = 1;
    return 0;
}
