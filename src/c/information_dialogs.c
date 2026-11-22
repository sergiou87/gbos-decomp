typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u16 fat_default_date(void);
extern u16 fat_default_time(void);
extern u8 read_register3(void);
extern u8 bios_version;
extern u8 os_version;
extern u16 input_header_marker;
extern void clear_browser_screen(void);
extern void display_set_attributes(u16 enabled);
extern void display_center(const u8 *text, u8 y);
extern void display_write_at(const u8 *text, u8 x, u8 y);
extern void display_decimal(u32 value);
extern void display_write(const u8 *text);
extern void display_print(const u8 *text);
extern void display_hex_word(u16 value);
extern void draw_screen(void);
extern void wait_keypress(void);

void device_information(void)
{
    u8 version;
    u16 date = fat_default_date();
    u16 time = fat_default_time();

    version = read_register3();

    clear_browser_screen();
    display_set_attributes(1);
    display_center((const u8 *)"System Information", 1);
    display_set_attributes(0);
    display_write_at((const u8 *)"CPLD Version:", 1, 4);
    display_decimal(version);
    display_write_at((const u8 *)"BIOS Version:", 1, 6);
    display_decimal(bios_version);
    display_write_at((const u8 *)"OS Version:  ", 1, 8);
    display_decimal(os_version);
    display_write_at((const u8 *)"Asm Date:", 1, 12);
    display_decimal(date & 0x1f);
    display_write((const u8 *)".");
    display_decimal((date >> 5) & 0x0f);
    display_write((const u8 *)".");
    display_decimal((date >> 9) + 1980);
    display_write_at((const u8 *)"Asm Time:", 1, 14);
    display_decimal(time >> 11);
    display_write((const u8 *)":");
    display_decimal((time >> 5) & 0x3f);
    display_write((const u8 *)":");
    display_decimal((time & 0x1f) * 2);
    draw_screen();
    wait_keypress();
}

void about_dialog(void)
{
    clear_browser_screen();
    display_set_attributes(0);
    display_print((const u8 *)"EverDrive GB");
    display_print((const u8 *)"");
    display_set_attributes(1);
    display_print((const u8 *)"Developed by:");
    display_set_attributes(0);
    display_print((const u8 *)"I. Golubovskiy");
    display_print((const u8 *)"");
    display_set_attributes(1);
    display_print((const u8 *)"Support:");
    display_set_attributes(0);
    display_print((const u8 *)"biokrik@gmail.com");
    display_print((const u8 *)"http://krikzz.com");
    display_print((const u8 *)"");
    display_set_attributes(1);
    display_print((const u8 *)"Control:");
    display_set_attributes(0);
    display_print((const u8 *)"LEF/RIG-switch page");
    display_print((const u8 *)"B-file menu");
    display_print((const u8 *)"A-back");
    display_print((const u8 *)"SELECT-main menu");
    display_print((const u8 *)"START-run last game");
    display_write_at((const u8 *)"SN:", 0, 17);
    display_hex_word(input_header_marker);
    draw_screen();
    wait_keypress();
}
