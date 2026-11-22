.module sd_read

.globl spi_stream_begin
.globl spi_stream_end
.globl sd_read_blocks
.globl sd_read_to_cartridge
.globl sd_wait_token
.globl decode_cartridge_bank

SELECT_READ = 0x0591
SELECT_BANK = 0x056b
SPI_DATA = 0xbe00

.area SD_READ (ABS)
.org 0x0417
sd_wait_token:
    push bc
    ld bc, #0x0000
wait_token_byte:
    ld a, (SPI_DATA)
    cp #0xfe
    jr z, token_received
    dec c
    jr nz, wait_token_byte
    dec b
    jr nz, wait_token_byte
    call spi_stream_end
    pop bc
    ld e, #0xd3
    ; Discard the reader's return address, propagating timeout to its caller.
token_received:
    pop bc
    ret

; Stack arguments: destination pointer, 8-bit count of 512-byte blocks.
sd_read_blocks:
    pop de
    pop hl
    pop bc
    push bc
    push hl
    push de
    call spi_stream_begin
    ld de, #SPI_DATA
    ld b, #0x00
read_block:
    ld a, (de)
    call sd_wait_token
read_pair:
    ld a, (de)
    ld (hl+), a
    ld a, (de)
    ld (hl+), a
    dec b
    jr nz, read_pair
    ld a, (de)
    dec c
    jr nz, read_block
    call spi_stream_end
    ld e, #0x00
    ret

sd_read_to_cartridge:
    ldhl sp, #2
    call decode_cartridge_bank
    ldhl sp, #4
    ld c, (hl)
    inc hl
    ld b, (hl)
    call SELECT_READ
    call spi_stream_begin
next_bank:
    call SELECT_BANK
    inc de
    ld hl, #0xa000
next_sector:
    ld a, (SPI_DATA)
    call sd_wait_token
    call read_sector_to_window
    dec bc
    ld a, b
    or c
    cp #0x00
    jr z, read_complete
    ld a, #0xb0
    cp h
    jr nz, next_sector
    jp next_bank
read_complete:
    call spi_stream_end
    ld e, #0x00
    ret

read_sector_to_window:
    push bc
    push de
    ld c, #0x80
    ld de, #SPI_DATA
read_quad:
    ld a, #0x56
    ld (0xaaaa), a
    ld a, (de)
    ld (hl+), a
    ld a, (de)
    ld (hl+), a
    ld a, (de)
    ld (hl+), a
    ld a, (de)
    ld (hl+), a
wait_write_ready:
    ld a, (0xbf00)
    sla a
    jr nc, wait_write_ready
    dec c
    jr nz, read_quad
    ld a, (de)
    pop de
    pop bc
    ret

; Dereference the pointer argument, then divide its 16-bit value by eight.
decode_cartridge_bank:
    ld e, (hl)
    inc hl
    ld d, (hl)
    push de
    pop hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ld a, d
    srl d
    srl d
    srl d
    srl e
    srl e
    srl e
    rr a
    rr a
    rr a
    rr a
    and #0xe0
    or e
    ld e, a
    ret
