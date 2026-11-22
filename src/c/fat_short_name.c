typedef unsigned char u8;
typedef unsigned int u16;

extern void memory_copy(const u8 *source, u8 *destination, u16 count);

u8 fat_make_short_name(const u8 *name, u8 *alias, u16 sequence)
{
    u8 length;
    u8 index;
    u8 extension;
    u8 character;
    u8 checksum;

    memory_copy(name, alias, 8);
    alias[8] = ' ';
    alias[9] = ' ';
    alias[10] = ' ';
    extension = 0;
    for (length = 0; (character = name[length]) != 0; length++)
        if (character == '.')
            extension = length;
    if (extension) {
        extension++;
        for (index = 0; index < 3; index++) {
            if (!name[index])
                break;
            alias[index + 8] = name[index + extension];
        }
    }
    for (index = 0; index < 11; index++) {
        if (alias[index] >= 'a' && alias[index] <= 'z')
            alias[index] &= 0xdf;
    }
    alias[4] = '~';
    alias[5] = sequence % 1000 / 100 + '0';
    alias[6] = sequence % 100 / 10 + '0';
    alias[7] = sequence % 10 + '0';
    checksum = 0;
    for (index = 0; index < 11; index++)
        checksum = ((checksum & 1) << 7 | (checksum & 0xfe) >> 1) + alias[index];
    return checksum;
}
