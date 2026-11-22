typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern void display_decimal_at(u32 value, u8 x, u8 y);
extern void display_put_char(u8 character);

void display_size(u32 size, u16 x, u16 y)
{
    u8 column;
    u8 row;

    row = y;
    column = x;
    if (size < 1024) {
        display_decimal_at(size, column, row);
        display_put_char('B');
    } else if (size < 0x100000) {
        display_decimal_at(size >> 10, column, row);
        display_put_char('K');
    } else {
        display_decimal_at(size >> 20, column, row);
        display_put_char('M');
    }
}
