typedef unsigned char u8;
typedef unsigned long u32;

extern void display_write(const u8 *text);

void display_decimal(u32 value)
{
    u8 buffer[11];
    u8 *text = buffer + 10;

    *text = 0;
    if (!value)
        *--text = '0';
    for (;;) {
        switch (value) {
        case 0:
            goto print;
        }
        *--text = value % 10 + '0';
        value /= 10;
    }
print:
    display_write(text);
}
