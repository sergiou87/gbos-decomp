typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

extern u8 fat_find_free_run(u32 *first, u16 count);
extern u8 fat_link_run(u32 cluster, u16 count);
extern u8 fat_replace_last_link(u32 cluster, u32 next);
extern u8 fat_zero_clusters(u32 cluster, u16 count);

u8 fat_allocate_chain(u32 last, u16 count, u8 clear)
{
    u32 first;
    u8 error;

    error = fat_find_free_run(&first, count);
    if (error)
        return error;
    error = fat_link_run(first, count);
    if (error)
        return error;
    error = fat_replace_last_link(last, first);
    if (error)
        return error;
    if (clear) {
        error = fat_zero_clusters(first, count);
        if (error)
            return error;
    }
    return 0;
}
