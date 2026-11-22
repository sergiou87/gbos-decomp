typedef unsigned char u8;
typedef unsigned int u16;

extern u16 input_keys;
extern u8 display_width(void);
extern u8 display_height(void);
extern void display_set_attributes(u16 enabled);
extern void display_write_at_n(const u8 *text, u8 x, u8 y, u16 length);
extern void display_center(const u8 *text, u8 y);
extern void draw_screen(void);
extern void wait_keypress(void);
extern void clear_browser_screen(void);
extern u8 menu_select(const u8 **items, u16 selected);
extern u16 heap_alloc(u16 size);
extern void heap_release(u16 size);
extern void memory_fill(void *destination, u8 value, u16 count);
extern void memory_copy(const u8 *source, u8 *destination, u16 count);
extern u8 cheat_code_valid(const u8 *code);
extern void cheat_edit_code(u8 *code);
extern void cheat_apply_codes(const u8 *codes);
extern const u8 cheat_menu_title[];
extern const u8 cheat_edit_label[];
extern const u8 cheat_remove_label[];
extern const u8 cheat_apply_title[];
extern const u8 cheat_apply_label[];
extern const u8 cheat_continue_label[];
extern const u8 cheat_exit_label[];
extern const u8 cheat_applying_label[];

void load_cheats(const u8 *data)
{
    u8 *code;
    u8 response;
    u8 *codes;
    u8 y;
    u8 count;
    u16 index;
    u8 x;
    u8 selected;
    u8 changed;
    const u8 *edit_menu[4];
    const u8 *exit_menu[5];

    x = (display_width() - 9) / 2;
    y = (display_height() - 16) / 2;
    selected = 0;
    changed = 0;
    edit_menu[0] = cheat_menu_title;
    edit_menu[1] = cheat_edit_label;
    edit_menu[2] = cheat_remove_label;
    edit_menu[3] = 0;
    exit_menu[0] = cheat_apply_title;
    exit_menu[1] = cheat_apply_label;
    exit_menu[2] = cheat_continue_label;
    exit_menu[3] = cheat_exit_label;
    exit_menu[4] = 0;
    codes = (u8 *)heap_alloc(144);
    memory_fill(codes, 'X', 144);
    clear_browser_screen();
    if (data) {
        changed = 1;
        code = codes;
        count = 0;
        for (index = 0; index < 512 && count < 16; index++) {
            if (data[index + 3] == '-' && cheat_code_valid(data + index)) {
                memory_copy(data + index, code, 3);
                memory_copy(data + (index + 4), code + 3, 3);
                if (data[index + 7] == '-') {
                    memory_copy(data + (index + 8), code + 6, 3);
                    index += 4;
                }
                index += 7;
                code += 9;
                count++;
            }
        }
    }
    for (;;) {
        for (index = 0; index < 16; index++) {
            if (index == selected)
                display_set_attributes(1);
            else
                display_set_attributes(0);
            display_write_at_n(codes + index * 9, x, y + index, 9);
        }
        draw_screen();
        wait_keypress();
        if (input_keys == 1) {
            if (changed) {
                response = menu_select(exit_menu, 0);
                clear_browser_screen();
                if (response == 0) {
                    display_set_attributes(0);
                    y = display_height() / 2;
                    display_center(cheat_applying_label, y);
                    draw_screen();
                    cheat_apply_codes(codes);
                }
                if (response == 1)
                    continue;
            }
            clear_browser_screen();
            heap_release(144);
            break;
        }
        if (input_keys == 0x40)
            selected = selected == 0 ? 15 : selected - 1;
        if (input_keys == 0x80)
            selected = selected == 15 ? 0 : selected + 1;
        if (input_keys == 2) {
            response = menu_select(edit_menu, 0);
            code = codes + selected * 9;
            if (response == 0) {
                cheat_edit_code(code);
                changed = 1;
            }
            if (response == 1)
                memory_fill(code, 'X', 9);
        }
    }
}
