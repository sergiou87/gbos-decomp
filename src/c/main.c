typedef unsigned char u8;

extern u8 os_initialize(void);
extern u8 recover_initialization(void);
extern void show_error(u8 error);
extern void poll_command_monitor(void);
extern u8 file_browser(void);

void main_entry(void)
{
    u8 error = os_initialize();
    if (error == 0xc0)
        error = recover_initialization();
    if (error) {
        show_error(error);
        for (;;) poll_command_monitor();
    }
    for (;;) {
        error = file_browser();
        show_error(error);
    }
}
