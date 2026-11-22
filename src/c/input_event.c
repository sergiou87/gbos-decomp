typedef unsigned char u8;
typedef unsigned int u16;

extern u8 os_boot_mode;
extern u16 input_header_marker;
extern u16 input_mix_counter;

void input_key_event(u8 keys)
{
    u8 *destination;

    if (os_boot_mode && input_header_marker == 0x0e1a) {
        input_mix_counter++;
        input_mix_counter += keys;
        destination = (u8 *)0x01d0 + (input_mix_counter & 0x1f);
        *destination = ((u8 *)0x0200)[input_mix_counter & 0x7fff];
    }
}
