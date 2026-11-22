.module startup

.globl boot_start
.globl alternate_boot
.globl initialize_cartridge
.globl _main_entry

BOOT_MARKER = 0x0200
INITIALIZE_CARTRIDGE = 0x02ca

.area STARTUP (ABS)
.org 0x0280
boot_start:
    di
    ld sp, #0xdff0
    ld c, a
    ld a, #0x00
    ldh (#0xff), a
    ldh (#0x41), a
    ld a, #0x80
    ldh (#0x40), a
    ld a, (BOOT_MARKER)
    cp #0xa5
    call nz, INITIALIZE_CARTRIDGE
    jp _main_entry

; Copy the OS image through the cartridge's 4 KiB window.
alternate_boot:
    ld a, #0x05
    ld (0xbf00), a
    ld hl, #0x0216
    ld de, #0xa216
    ld c, #0x08
restore_bank:
    ld a, c
    ld (0xbf80), a
    inc c
restore_byte:
    ld a, (de)
    ld (hl+), a
    inc e
    jr nz, restore_byte
    inc d
    ld a, d
    cp #0xb0
    jr nz, restore_byte
    ld de, #0xa000
    ld a, c
    cp #0x10
    jr nz, restore_bank
    ld a, #0x01
    ld (0x0201), a
    ld sp, #0xdff0
    jp _main_entry

initialize_cartridge:
    ld a, #0x04
    ld (0xbf00), a
    ld a, #0x02
    ld (0xbf80), a
    ld b, #0x18
    ld hl, #0x0000
    ld a, #0x05
    ld (0xbf00), a
backup_bank:
    ld a, b
    ld (0xbf80), a
    inc b
    ld de, #0xa000
backup_byte:
    ld a, (hl+)
    ld (de), a
    inc de
    ld a, d
    cp #0xb0
    jr nz, backup_byte
    ld a, b
    cp #0x20
    jr nz, backup_bank
    ld a, #0x04
    ld (0xbf00), a
    ld a, #0x03
    ld (0xbf80), a
    ld a, #0xa5
    ld (BOOT_MARKER), a
    ld a, c
    ld (0x0212), a
    ret
