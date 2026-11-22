typedef unsigned char u8;
typedef unsigned int u16;

extern u8 heap_pool[0x1800];
extern u16 heap_remaining;
extern void show_error(u8 error);
extern void poll_command_monitor(void);

u16 heap_alloc(u16 size)
{
    if (heap_remaining < size) {
        show_error(0x35);
        for (;;)
            poll_command_monitor();
    }
    heap_remaining -= size;
    return (u16)(heap_pool + heap_remaining);
}

void heap_release(u16 size)
{
    heap_remaining += size;
    if (heap_remaining > sizeof(heap_pool)) {
        show_error(0x36);
        for (;;)
            poll_command_monitor();
    }
}
