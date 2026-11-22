.module sd_write

.globl sd_write_blocks

SPI_DATA = 0xbe00

.area SD_WRITE (ABS)
.org 0x062b
; Stack arguments: source pointer, 8-bit count of 512-byte blocks.
sd_write_blocks:
    pop de
    pop hl
    pop bc
    push bc
    push hl
    push de
    ld b, #0x00
write_block:
    ld de, #SPI_DATA
    ld a, #0xff
    ld (de), a
    ld (de), a
    ld a, #0xfc
    ld (de), a
write_pair:
    ld a, (hl+)
    ld (de), a
    ld a, (hl+)
    ld (de), a
    dec b
    jr nz, write_pair
    ld a, #0xff
    ld (de), a
    ld (de), a
    ld (de), a
    ld a, (de)
    and #0x1f
    cp #0x05
    jr nz, rejected
    ld de, #0x0000
wait_block_ready:
    ld a, #0xff
    ld (SPI_DATA), a
    ld a, (SPI_DATA)
    cp #0xff
    jr z, block_done
    dec d
    jr nz, wait_block_ready
    dec e
    jr nz, wait_block_ready
    jr block_timeout
block_done:
    dec c
    jr nz, write_block
    ld a, #0xfd
    ld (SPI_DATA), a
    ld a, #0xff
    ld (SPI_DATA), a
    ld de, #0x0000
wait_stop_ready:
    ld a, #0xff
    ld (SPI_DATA), a
    ld a, (SPI_DATA)
    and #0x01
    cp #0x00
    jr nz, complete
    inc d
    jr nz, wait_stop_ready
    inc e
    jr nz, wait_stop_ready
    jr stop_timeout
complete:
    ld e, #0x00
    ret
rejected:
    ld e, #0xd5
    ret
block_timeout:
    ld e, #0xd6
    ret
stop_timeout:
    ld e, #0xd7
    ret
