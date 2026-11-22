typedef unsigned char u8;
typedef unsigned int u16;

extern u8 display_attributes;

u8 display_page_rows(void)
{
    return 12;
}

u8 display_width(void)
{
    return 20;
}

u8 display_height(void)
{
    return 18;
}

void display_set_attributes(u16 enabled)
{
    display_attributes = enabled == 0 ? 0 : 0x80;
}
