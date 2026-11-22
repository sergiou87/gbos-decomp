typedef unsigned char u8;

/* Root-directory back action is a no-op in this firmware. */
u8 browser_exit(void)
{
    return 0;
}
