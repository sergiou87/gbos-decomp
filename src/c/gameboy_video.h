#ifndef GBOS_GAMEBOY_VIDEO_H
#define GBOS_GAMEBOY_VIDEO_H

#define REG_LCDC (*(volatile u8 *)0xff40)
#define REG_STAT (*(volatile u8 *)0xff41)
#define REG_BGP (*(volatile u8 *)0xff47)
#define REG_BGPI (*(volatile u8 *)0xff68)
#define REG_BGPD (*(volatile u8 *)0xff69)

#define LCDC_ENABLE 0x80
#define STAT_MODE_MASK 3
#define STAT_MODE_VBLANK 1
#define CGB_BACKGROUND_COLORS 32
#define CGB_PALETTE_COLOR_MASK 3

#endif
