#ifndef GBOS_SETTINGS_H
#define GBOS_SETTINGS_H

typedef struct {
    u8 mapper;
    u8 rom_size;
    u8 ram_size;
    u8 save_enabled;
    u8 swap_buttons;
    u8 name[208];
    u16 signature;
} Settings;

#define SETTINGS_BANK 23
#define SETTINGS_SIGNATURE 0xaa55
#define MAPPER_MBC1 1
#define MAPPER_MBC2 2
#define MAPPER_MBC3 3
#define MAPPER_ROM 4
#define MAPPER_MBC5 5
#define ROM_SIZE_MAX 7
#define RAM_SIZE_8K 1
#define RAM_SIZE_32K 3

#define FLASH_ERASE_SECTORS 128
#define FLASH_ERASE_MAX_GROUPS 127
#define FLASH_MAX_SECTORS 0x3f80
#define ROM_SIZE_MIN_BYTES 65536
#define ROM_SIZE_MAX_BYTES 0x800000
#define ROM_HEADER_MAPPER 0xa147
#define ROM_HEADER_RAM_SIZE 0xa149

#endif
