.module monitor

.globl boot_start
.globl command_monitor

FIFO_PORT = 0xbe80
RECEIVE_BYTE = 0x04cc
SEND_D = 0x04de

.area MONITOR (ABS)
.org 0x0307
command_monitor:
    di
    ld sp, #0xdff0
wait_prefix:
    call RECEIVE_BYTE
    cp #'=
    jr nz, wait_prefix
    call RECEIVE_BYTE
    cp #'R
    jr z, read_byte
    cp #'W
    jr z, write_byte
    cp #'t
    jr z, identify
    cp #'a
    jr z, set_address
    cp #'r
    jr z, read_block
    cp #'w
    jr z, write_block
    cp #'s
    jr z, restart
    jp wait_prefix
write_byte:
    call RECEIVE_BYTE
    ld (hl), a
    jp wait_prefix
read_byte:
    ld d, (hl)
    call SEND_D
    jp wait_prefix
identify:
    ld d, #'k
    call SEND_D
    jp wait_prefix
set_address:
    call RECEIVE_BYTE
    ld h, a
    call RECEIVE_BYTE
    ld l, a
    jp wait_prefix
write_block:
    call RECEIVE_BYTE
    ld c, a
    ld b, #0x00
    ld de, #FIFO_PORT
wait_receive:
    ld a, #0xa0
    ld (de), a
    nop
    nop
    nop
    ld a, (de)
    and #0x02
    jr nz, write_block
    ld a, #0xa1
    ld (de), a
    nop
    nop
    nop
    ld a, (de)
    ld (hl+), a
    dec b
    jr nz, wait_receive
    dec c
    jr nz, wait_receive
    jp wait_prefix
read_block:
    call RECEIVE_BYTE
    ld c, a
    ld b, #0x00
    ld de, #FIFO_PORT
wait_transmit:
    ld a, #0xa0
    ld (de), a
    nop
    nop
    nop
    ld a, (de)
    and #0x01
    ; The original branches to write_block here, not read_block.
    jr nz, write_block
    ld a, #0xa2
    ld (de), a
    ld a, (hl+)
    nop
    ld (de), a
    dec b
    jr nz, wait_transmit
    dec c
    jr nz, wait_transmit
    jp wait_prefix
restart:
    jp boot_start
