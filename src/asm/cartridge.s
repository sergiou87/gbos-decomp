.module cartridge

.globl flash_erase
.globl window_select_bank
.globl window_mode1
.globl window_mode3
.globl launch_game
.globl select_mode3_bank
.globl select_mode1_bank
.globl set_register0
.globl set_register1
.globl fill_window
.globl check_window
.globl window_copy_program
.globl window_copy
.globl read_register3
.globl cartridge_noop
.globl flash_patch_boot
.globl flash_restore_boot
.globl copy_code_to_wram

REG_SELECT = 0xbf00
REG_DATA = 0xbf80
WINDOW = 0xa000
BANK_LOW = 0x0215
BANK_OTHER = 0x0214

.area CARTRIDGE (ABS)
.org 0x0515
flash_erase:
    ldhl sp, #4
    ld e, (hl)
    sla e
    sla e
    sla e
    sla e
    ld d, (hl)
    srl d
    srl d
    srl d
    srl d
    ldhl sp, #6
    ld b, (hl)
    call window_mode1
    call window_select_bank
    ld a, #0xaa
    ld (0xaaaa), a
    ld a, #0x55
    ld (0xa555), a
    ld a, #0x80
    ld (0xaaaa), a
    ld a, #0xaa
    ld (0xaaaa), a
    ld a, #0x55
    ld (0xa555), a
erase_group:
    ld c, #0x08
    call window_select_bank
erase_sector:
    ld a, #0x30
    ld (WINDOW), a
    inc de
    inc de
    ld a, e
    ld (REG_DATA), a
    dec c
    jr nz, erase_sector
    dec b
    jr nz, erase_group
    ld b, #0x00
erase_delay:
    nop
    nop
    dec b
    jr nz, erase_delay
    jp flash_wait_ready
window_select_bank:
    ld a, #0x06
    ld (REG_SELECT), a
    ld a, d
    ld (REG_DATA), a
    ld a, #0x05
    ld (REG_SELECT), a
    ld a, e
    ld (REG_DATA), a
    ret
flash_wait_ready:
    ld a, (REG_SELECT)
    sla a
    jr nc, flash_wait_ready
    ret
flash_wait_stable:
    ld a, (WINDOW)
read_again:
    ld b, a
    ld a, (WINDOW)
    cp b
    jr nz, read_again
    ret
window_mode1:
    ld a, #0x04
    ld (REG_SELECT), a
    ld a, #0x01
    ld (REG_DATA), a
    ret
window_mode3:
    ld a, #0x04
    ld (REG_SELECT), a
    ld a, #0x03
    ld (REG_DATA), a
    ret
launch_game:
    ldhl sp, #2
    ld c, (hl)
    ld a, #0x00
    ld (0x0000), a
    ld (0x2100), a
    ld (0x3000), a
    ld (0x4000), a
    ld (0x6000), a
    ld a, #0x01
    ld (0x2000), a
    ld hl, #launch_game_stub
    jp copy_code_to_wram
launch_game_stub:
    ld a, #0x07
    ld (REG_SELECT), a
    ld a, c
    or #0x08
    ld (REG_DATA), a
    ld a, #0x11
    jp 0x0100
select_mode3_bank:
    call window_mode3
    jp select_bank_argument
select_mode1_bank:
    call window_mode1
select_bank_argument:
    pop hl
    pop de
    push de
    push hl
    call window_select_bank
    ret
set_register0:
    ldhl sp, #2
    ld a, #0x00
    ld (REG_SELECT), a
    ld a, (hl)
    ld (REG_DATA), a
    ret
set_register1:
    ldhl sp, #2
    ld a, #0x01
    ld (REG_SELECT), a
    ld a, (hl)
    ld (REG_DATA), a
    ret
fill_window:
    ldhl sp, #2
    ld a, (hl)
    ld bc, #0x0400
    ld hl, #WINDOW
fill_quad:
    ld (hl+), a
    ld (hl+), a
    ld (hl+), a
    ld (hl+), a
    dec c
    jr nz, fill_quad
    dec b
    jr nz, fill_quad
    ret
check_window:
    ldhl sp, #2
    ld d, (hl)
    ld bc, #0x1000
    ld hl, #WINDOW
    ld e, #0x00
check_byte:
    ld a, (hl+)
    cp d
    jr nz, check_complete
    dec c
    jr nz, check_byte
    dec b
    jr nz, check_byte
    ld e, #0x01
check_complete:
    ret

.org 0x0699
; Save two low bank numbers; H supplies their shared high byte.
save_copy_banks:
    ld a, #0x06
    ld (REG_SELECT), a
    ld a, h
    ld (REG_DATA), a
    ld a, l
    ld (BANK_LOW), a
    ld a, c
    ld (BANK_OTHER), a
    ret
select_copy_other:
    call window_mode3
    ld a, #0x05
    ld (REG_SELECT), a
    ld a, (BANK_OTHER)
    ld (REG_DATA), a
    ret
select_copy_low:
    call window_mode1
    ld a, #0x05
    ld (REG_SELECT), a
    ld a, (BANK_LOW)
    ld (REG_DATA), a
    ret

; The 0x7f00 page is used as scratch while exchanging cartridge windows.
window_copy_program:
    pop de
    pop hl
    pop bc
    push bc
    push hl
    push de
    call save_copy_banks
    ld hl, #WINDOW
    ld de, #0x7f00
program_page:
    call select_copy_other
stage_program_page:
    ld a, (hl)
    ld (de), a
    inc l
    inc e
    jr nz, stage_program_page
    call select_copy_low
program_quad:
    ld a, #0x56
    ld (0xaaaa), a
    ld a, (de)
    ld (hl+), a
    inc e
    ld a, (de)
    ld (hl+), a
    inc e
    ld a, (de)
    ld (hl+), a
    inc e
    ld a, (de)
    ld (hl+), a
program_wait:
    ld a, (REG_SELECT)
    sla a
    jr nc, program_wait
    inc e
    jr nz, program_quad
    ld a, h
    cp #0xb0
    jr nz, program_page
    ret
window_copy:
    pop de
    pop hl
    pop bc
    push bc
    push hl
    push de
    call save_copy_banks
    ld hl, #WINDOW
    ld de, #0x7f00
copy_page:
    call select_copy_low
stage_copy_page:
    ld a, (hl)
    ld (de), a
    inc l
    inc e
    jr nz, stage_copy_page
    call select_copy_other
copy_byte:
    ld a, (de)
    ld (hl+), a
    inc e
    jr nz, copy_byte
    ld a, h
    cp #0xb0
    jr nz, copy_page
    ret
read_register3:
    ld a, #0x03
    ld (REG_SELECT), a
    ld a, (REG_DATA)
    ld e, a
    ret
cartridge_noop:
    ret

flash_patch_boot:
    ld hl, #patch_boot_stub
    jr copy_code_to_wram
patch_boot_stub:
    ld a, #0x04
    ld (REG_SELECT), a
    ld a, #0x04
    ld (REG_DATA), a
    ld a, #0xaa
    ld (0x0aaa), a
    ld a, #0x55
    ld (0x0555), a
    ld a, #0xa0
    ld (0x0aaa), a
    ld a, #0xfe
    ld (0x0208), a
patch_wait:
    ld a, (REG_SELECT)
    sla a
    jr nc, patch_wait
    ld a, #0x00
    ld (REG_DATA), a
    jp 0x0100

flash_restore_boot:
    ld a, #0x00
    ld (0x0200), a
    ld (0x0201), a
    ld (0x0212), a
    ld a, #0x02
    ld (0x0202), a
    ld a, #0xff
    ld (0x0203), a
    ld (0x0208), a
    ld (0x0209), a
    ld hl, #restore_boot_stub
copy_code_to_wram:
    ld de, #0xc000
copy_code_byte:
    ld a, (hl+)
    ld (de), a
    inc e
    jr nz, copy_code_byte
    jp 0xc000

restore_boot_stub:
    ld a, #0x04
    ld (REG_SELECT), a
    ld de, #0x0aaa
    ld a, #0x04
    ld (REG_DATA), a
    ld a, #0xaa
    ld (de), a
    ld a, #0x55
    ld (0x0555), a
    ld a, #0x80
    ld (de), a
    ld a, #0xaa
    ld (de), a
    ld a, #0x55
    ld (0x0555), a
    ld a, #0x30
    ld (0x0000), a
    ld (0x2000), a
    ld (0x4000), a
    ld (0x6000), a
restore_erase_wait:
    ld a, (REG_SELECT)
    sla a
    jr nc, restore_erase_wait
    ld hl, #0x0000
restore_boot_byte:
    ld a, #0x03
    ld (REG_DATA), a
    ld b, (hl)
    ld a, #0x04
    ld (REG_DATA), a
    ld a, #0xaa
    ld (de), a
    ld a, #0x55
    ld (0x0555), a
    ld a, #0xa0
    ld (de), a
    ld (hl), b
restore_program_wait:
    ld a, (REG_SELECT)
    sla a
    jr nc, restore_program_wait
    inc l
    jr nz, restore_boot_byte
    inc h
    ld a, h
    cp #0x80
    jr nz, restore_boot_byte
    ld a, #0x00
    ld (REG_DATA), a
    jp 0x0100
