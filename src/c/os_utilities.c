typedef unsigned char u8;
typedef unsigned int u16;

extern u8 menu_select(const u8 **items, u16 selected);
extern void settings_menu(void);
extern void load_cheats(const u8 *data);
extern void device_information(void);
extern void about_dialog(void);
extern void clear_browser_screen(void);
extern u16 fat_date;
extern u16 fat_time;
extern u8 fifo_receive_busy(void);
extern u8 fifo_receive(void);
extern void fifo_send(u8 value);
extern void command_monitor(void);

u8 main_menu(void)
{
    u8 selected = 0;
    const u8 *items[] = {
        (const u8 *)"Main Menu", (const u8 *)"Options", (const u8 *)"Cheats",
        (const u8 *)"Device Info", (const u8 *)"About", 0
    };

    for (;;) {
        selected = menu_select(items, selected);
        if (selected == 0xff)
            return 0;
        if (selected == 0)
            settings_menu();
        if (selected == 1)
            load_cheats(0);
        if (selected == 2)
            device_information();
        if (selected == 3)
            about_dialog();
        clear_browser_screen();
    }
}

u16 fat_default_date(void)
{
    return fat_date;
}

u16 fat_default_time(void)
{
    return fat_time;
}

void poll_command_monitor(void)
{
    u8 command;

    if (fifo_receive_busy())
        return;
    command = fifo_receive();
    if (command != '=')
        return;
    command = fifo_receive();
    switch (command) {
    case 't':
        fifo_send('k');
        break;
    case 'L':
        command_monitor();
        break;
    }
}
