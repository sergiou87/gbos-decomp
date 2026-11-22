typedef unsigned char u8;
typedef unsigned int u16;

#include "gameboy_video.h"

#define CGB_PALETTE_PROBE 0xaa
#define DISPLAY_DMG_PALETTE 0xdc
#define DISPLAY_COLOR_WHITE 0xffff
#define DISPLAY_COLOR_BLACK 0
#define DISPLAY_COLOR_LIGHT 0x7373
#define FONT_ROM_ADDRESS 0x7d00
/* LCD and BG on, unsigned tiles, 8x16 OBJ size selected; OBJ/window stay off. */
#define DISPLAY_LCD_CONFIGURATION 0x95

extern u8 cgb_detected;
extern u8 display_attributes;
extern void wait_vblank(void);
extern void load_font(const u8 *data);
extern void clear_browser_screen(void);

void display_initialize(void)
{
    u8 color_index;
    /* Keep the original stored words, including unused color bit 15. */
    static const u16 background_palette[] = {
        DISPLAY_COLOR_WHITE, DISPLAY_COLOR_BLACK,
        DISPLAY_COLOR_LIGHT, DISPLAY_COLOR_BLACK
    };

    cgb_detected = 0;
    wait_vblank();
    /* DMG hardware ignores writes to CGB palette RAM. */
    REG_BGPI = 0;
    REG_BGPD = CGB_PALETTE_PROBE;
    REG_BGPI = 0;
    if (REG_BGPD == CGB_PALETTE_PROBE)
        cgb_detected = 1;
    REG_LCDC = LCDC_ENABLE;
    REG_BGP = DISPLAY_DMG_PALETTE;
    if (cgb_detected) {
        for (color_index = 0; color_index < CGB_BACKGROUND_COLORS; color_index++) {
            while ((REG_STAT & STAT_MODE_MASK) != STAT_MODE_VBLANK)
                ;
            REG_BGPI = color_index * 2;
            REG_BGPD = background_palette[color_index & CGB_PALETTE_COLOR_MASK] & 0xff;
            REG_BGPI = color_index * 2 + 1;
            REG_BGPD = background_palette[color_index & CGB_PALETTE_COLOR_MASK] >> 8;
        }
    }
    display_attributes = 0;
    load_font((const u8 *)FONT_ROM_ADDRESS);
    clear_browser_screen();
    REG_LCDC = DISPLAY_LCD_CONFIGURATION;
}
