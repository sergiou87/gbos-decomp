typedef unsigned char u8;
typedef unsigned int u16;

struct cheat_table {
    u16 address[16];
    u16 bank[16];
    u8 value[16];
};

extern u16 heap_alloc(u16 size);
extern void heap_release(u16 size);
extern void memory_fill(void *destination, u8 value, u16 length);
extern u8 cheat_hex_digit(u8 character);
extern u16 cheat_code_address(const u8 *code);
extern void select_mode1_bank(u16 bank);
extern void cheat_patch_codes(struct cheat_table *table, u8 count);

void cheat_apply_codes(const u8 *codes)
{
    register u8 length;
    u16 bank;
    u16 address;
    const u8 *code = 0;
    u8 reference;
    u8 count;
    u8 index;
    struct cheat_table *table;

    table = (struct cheat_table *)(void *)heap_alloc(sizeof(struct cheat_table));
    memory_fill(table, 0, sizeof(struct cheat_table));
    count = 0;
    for (index = 0; index < 16; index++) {
        code = codes + index * 9;
        for (length = 0; length < 9; length++)
            if (code[length] == 'X')
                break;
        if (length != 9 && length != 6)
            continue;
        table->bank[count] = 0;
        table->address[count] = cheat_code_address(code);
        if (table->address[count] == 0xffff)
            continue;
        table->value[count] = cheat_hex_digit(code[0]) << 4 | cheat_hex_digit(code[1]);
        if (length == 9) {
            reference = cheat_hex_digit(code[6]) << 4 | cheat_hex_digit(code[8]);
            reference ^= 0xff;
            length = reference;
            reference >>= 2;
            length <<= 6;
            reference |= length;
            reference ^= 0x45;
            address = table->address[count];
            if (address & 0xc000) {
                bank = address >> 12;
                address &= 0x0fff;
                code = (u8 *)(0xa000 + address);
                for (; !(bank & 0xfc00); bank += 4) {
                    select_mode1_bank(bank);
                    if (reference == *code)
                        break;
                }
                if (bank >= 0x400)
                    continue;
                table->bank[count] = bank;
            } else {
                table->bank[count] = address >> 12;
            }
        }
        count++;
    }
    cheat_patch_codes(table, count);
    heap_release(sizeof(struct cheat_table));
}
