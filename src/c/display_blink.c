typedef unsigned char u8;
typedef unsigned int u16;

extern u16 display_blink_counter;
extern void display_write_at(const u8 *text, u8 x, u8 y);

void display_blink(const u8 *text, u16 x, u16 y)
{
    display_blink_counter++;
    if ((display_blink_counter & 15) < 8)
        display_write_at(text, x, y);
    else
        display_write_at((const u8 *)" ", x, y);
}
