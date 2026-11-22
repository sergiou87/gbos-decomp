#include "types.h"
#include "bios.h"
#include "fat.h"
#include "str.h"
#include "disk.h"
#include "sys.h"

struct cheat_table {
    u16 address[16];
    u16 bank[16];
    u8 value[16];
};

extern void cheat_stage_bank(u8 bank);
extern void cheat_program_bank(u8 bank);
extern void cheat_write_staged_byte(u16 address, u8 value);
extern void settings_store(void);

void cheat_patch_codes(struct cheat_table *table, u8 count)
{
    u8 other;
    u8 index;
    u32 offset;
    u8 bank;

    for (index = 0; index < count; index++) {
        if (!table->bank[index] && !table->address[index])
            continue;
        offset = table->bank[index];
        offset <<= 12;
        offset += table->address[index] & 0x0fff;
        bank = offset >> 16;
        cheat_stage_bank(bank);
        cheat_write_staged_byte(offset & 0xffff, table->value[index]);
        for (other = index + 1; other < count; other++) {
            offset = table->bank[other];
            offset <<= 12;
            offset += table->address[other] & 0x0fff;
            if ((offset >> 16) == bank) {
                cheat_write_staged_byte(offset & 0xffff, table->value[other]);
                table->bank[other] = 0;
                table->address[other] = 0;
            }
        }
        cheat_program_bank(bank);
    }
    settings_store();
}
