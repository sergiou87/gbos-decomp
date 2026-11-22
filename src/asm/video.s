.module video

.globl wait_vblank
.globl load_font
.globl draw_screen
.globl clear_screen
.globl normal_speed
.globl double_speed
.globl read_joypad
.globl _draw_screen
.globl _double_speed
.globl _normal_speed
.globl _clear_screen
.globl _wait_vblank
.globl _read_joypad
.globl _load_font
.globl font_fill_byte

SCREEN_BUFFER = 0xd85f
STAT = 0x41
JOYP = 0x00
KEY1 = 0xff4d

.area FONT_DATA (ABS)
.org 0x0819
font_fill_byte:
    .db 0x00

.area VIDEO (ABS)
.org 0x081a
wait_vblank:
_wait_vblank:
    ldh a, (#STAT)
    and #0x03
    cp #0x01
    jr z, wait_vblank
wait_vblank_start:
    ldh a, (#STAT)
    and #0x03
    cp #0x01
    jr nz, wait_vblank_start
    ret

; Stack argument: pointer to 1bpp font. Expand into both 2bpp palettes.
load_font:
_load_font:
    pop hl
    pop de
    push de
    push hl
    push de
    ld a, #0x00
    ld (font_fill_byte), a
    ld hl, #0x8200
    ld bc, #0x0300
    call expand_font
    ld a, #0xff
    ld (font_fill_byte), a
    ld hl, #0x8a00
    ld bc, #0x0300
    pop de
    call expand_font
    ret
expand_font:
    ldh a, (#STAT)
    and #0x03
    cp #0x01
    jr nz, expand_font
    ld a, (de)
    ld (hl+), a
    ld a, (font_fill_byte)
    ld (hl+), a
    inc de
    dec c
    jr nz, expand_font
    dec b
    jr nz, expand_font
    ret

draw_screen:
_draw_screen:
    ld bc, #0x9800
    ld hl, #SCREEN_BUFFER
    ld d, #20
draw_tile:
    ldh a, (#STAT)
    and #0x03
    cp #0x01
    jr nz, draw_tile
    ld a, (hl+)
    ld (bc), a
    inc c
    dec d
    jr nz, draw_tile
    ld d, #20
    ld a, c
    add #12
    ld c, a
    jr nz, draw_tile
    inc b
    ld a, b
    cp #0x9b
    jr nz, draw_tile
    ret

clear_screen:
_clear_screen:
    ld hl, #SCREEN_BUFFER
    ld b, #180
    ld a, #0x20
clear_pair:
    ld (hl+), a
    ld (hl+), a
    dec b
    jr nz, clear_pair
    ret

double_speed:
_double_speed:
    ld a, (KEY1)
    sla a
    jr nc, switch_speed
    ret
normal_speed:
_normal_speed:
    ld a, (KEY1)
    sla a
    jr c, switch_speed
    ret
switch_speed:
    ld a, #0x00
    ld (0xffff), a
    ld a, #0x30
    ld (0xff00), a
    ld a, #0x01
    ld (KEY1), a
    stop
    ret

read_joypad:
_read_joypad:
    ld a, #0x20
    ldh (#JOYP), a
    ldh a, (#JOYP)
    ldh a, (#JOYP)
    ldh a, (#JOYP)
    ldh a, (#JOYP)
    and #0x0f
    ld e, a
    ld a, #0x10
    ldh (#JOYP), a
    sla e
    sla e
    sla e
    sla e
    ldh a, (#JOYP)
    ldh a, (#JOYP)
    ldh a, (#JOYP)
    ldh a, (#JOYP)
    and #0x0f
    or e
    cpl
    ld e, a
    ld a, #0x30
    ldh (#JOYP), a
    ret
