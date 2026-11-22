typedef unsigned char u8;
typedef unsigned int u16;

extern void display_set_attributes(u16 enabled);
extern void display_fill(u16 tile, u8 x, u8 y, u8 width, u8 height);

void display_frame(u16 x, u16 y, u16 width, u16 height)
{
    u8 row;
    u8 top;
    u8 border_height;
    u8 border_width;
    u8 left;

    display_set_attributes(1);
    border_height = height + 2;
    border_width = width + 2;
    row = y;
    top = row - 1;
    left = x - 1;
    display_fill(' ', left, top, border_width, border_height);
    display_fill('-', left, row, border_width, 1);
    display_set_attributes(0);
}
