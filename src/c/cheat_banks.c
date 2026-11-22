typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern void window_copy(u16 source, u8 destination);
extern void window_copy_program(u16 destination, u8 source);
extern void flash_erase(u32 address, u16 groups);
extern void select_mode3_bank(u16 bank);

void cheat_stage_bank(u8 bank)
{
    u16 source = bank * 16;
    u8 index;
    u8 destination = 8;

    for (index = 0; index < 16; index++) {
        window_copy(source, destination);
        source++;
        destination++;
    }
}

void cheat_program_bank(u8 bank)
{
    u8 index;
    u8 source;
    u16 destination;

    if (bank > 63)
        return;
    destination = bank;
    destination <<= 4;
    flash_erase((u32)bank << 16, 1);
    source = 8;
    for (index = 0; index < 16; index++) {
        window_copy_program(destination, source);
        destination++;
        source++;
    }
}

void cheat_write_staged_byte(u16 address, u8 value)
{
    select_mode3_bank((address >> 12) + 8);
    *(u8 *)(0xa000 + (address & 0x0fff)) = value;
}
