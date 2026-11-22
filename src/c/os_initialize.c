typedef unsigned char u8;
typedef unsigned int u16;
typedef struct FatFile FatFile;

extern void spi_initialize(void);
extern void display_initialize(void);
extern void settings_initialize(void);
extern void display_print(const u8 *text);
extern void draw_screen(void);
extern void double_speed(void);
extern void prepare_os_reload(void);
extern void alternate_boot(void);
extern void select_mode3_bank(u16 bank);
extern u8 fat_open_path(const u8 *path, FatFile *entry, u16 mode);
extern u8 fat_read_file(u8 *buffer, u16 count, u8 mode);
extern u8 sd_initialize(void);
extern u8 fat_initialize(void);
extern u16 heap_alloc(u16 size);
extern void heap_release(u16 size);
extern u16 heap_remaining;
extern u8 *os_work_buffer;
extern u8 cgb_detected;
extern u8 os_boot_mode;
extern u8 fat_sectors_per_cluster;
extern const u8 os_patch_source[3];

u8 os_reload(FatFile *entry);

u8 os_initialize(void)
{
    u8 error;

    spi_initialize();
    heap_remaining = 0x1800;
    display_initialize();
    os_work_buffer = (u8 *)heap_alloc(0xd7);
    settings_initialize();
    if (cgb_detected)
        double_speed();
    display_print((const u8 *)"init...");
    draw_screen();
    error = sd_initialize();
    if (error)
        return error;
    error = fat_initialize();
    if (error)
        return error;
    if (fat_sectors_per_cluster < 8)
        return 0xef;
    if (!os_boot_mode) {
        prepare_os_reload();
        display_print((const u8 *)"OS loading...");
        draw_screen();
        error = os_reload((FatFile *)heap_alloc(0xe5));
        heap_release(0xe5);
        if (error)
            return error;
    } else if (*(u8 *)0x02a1 == 8) {
        *(u8 *)0x02a0 = 0x16;
        *(u8 *)0x02a1 = 0x02;
        *(u8 *)0x02a3 = 0x16;
        *(u8 *)0x02a4 = 0xa2;
        alternate_boot();
    } else if (os_patch_source[0] != 0xab) {
        ((u8 *)0x4b34)[0] = os_patch_source[0];
        ((u8 *)0x4b34)[1] = os_patch_source[1];
        ((u8 *)0x4b34)[2] = os_patch_source[2];
    }
    display_print((const u8 *)"ok ");
    draw_screen();
    return 0;
}

u8 os_reload(FatFile *entry)
{
    u8 error;
    u8 bank;

    error = fat_open_path((const u8 *)"/EDGB/GBOS    GB", entry, 0);
    if (error == 0xf0)
        return 0;
    if (error)
        return error;
    for (bank = 0; bank < 8; bank++) {
        select_mode3_bank(bank + 8);
        error = fat_read_file((u8 *)0xa000, 8, 1);
        if (error)
            return error;
    }
    alternate_boot();
    return 0x39;
}
