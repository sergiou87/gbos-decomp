.module spi

.globl spi_initialize
.globl spi_exchange
.globl spi_exchange_nowait
.globl spi_set_bit1
.globl spi_clear_bit1
.globl spi_set_bit0
.globl spi_wait_ready
.globl spi_stream_begin
.globl spi_stream_end
.globl fifo_receive
.globl fifo_send
.globl fifo_send_d
.globl fifo_receive_busy
.globl fifo_transmit_busy

REG_SELECT = 0xbf00
REG_DATA = 0xbf80
SPI_DATA = 0xbe00
SPI_CONFIG = 0x0213
FIFO_PORT = 0xbe80

.area SPI (ABS)
.org 0x03a0
spi_initialize:
    ld a, #0x02
    ld (REG_SELECT), a
    ld a, #0x02
    ld (REG_DATA), a
    ld (SPI_CONFIG), a
    ret
spi_exchange:
    ldhl sp, #2
    ld a, (hl)
    ld (SPI_DATA), a
    call spi_wait_ready
    ld a, (SPI_DATA)
    ld e, a
    ret
spi_exchange_nowait:
    ldhl sp, #2
    ld a, (hl)
    ld (SPI_DATA), a
    ld a, (SPI_DATA)
    ld e, a
    ret

; 0x03c7-0x03cd is compiled from spi_send.c.
.org 0x03ce
spi_set_bit1:
    ld a, (SPI_CONFIG)
    or #0x02
    jp spi_apply_config
spi_clear_bit1:
    ld a, (SPI_CONFIG)
    and #0xfd
    jp spi_apply_config
spi_set_bit0:
    ld a, (SPI_CONFIG)
    or #0x01
    jp spi_apply_config
spi_apply_config:
    ld (SPI_CONFIG), a
    ld a, #0x02
    ld (REG_SELECT), a
    ld a, (SPI_CONFIG)
    ld (REG_DATA), a
    ret
spi_wait_ready:
    ld a, (REG_SELECT)
    and #0x40
    jr z, spi_wait_ready
    ret
spi_stream_begin:
    ld a, #0x02
    ld (REG_SELECT), a
    ld a, (SPI_CONFIG)
    or #0x04
    ld (REG_DATA), a
    ret
spi_stream_end:
    ld a, #0x02
    ld (REG_SELECT), a
    ld a, (SPI_CONFIG)
    ld (REG_DATA), a
    ret

.area FIFO (ABS)
.org 0x04cc
fifo_receive:
    call fifo_wait_receive
    ld a, #0xa1
    ld (FIFO_PORT), a
    nop
    nop
    ld a, (FIFO_PORT)
    ld e, a
    ret
fifo_send:
    ldhl sp, #2
    ld d, (hl)
fifo_send_d:
    call fifo_wait_transmit
    ld a, #0xa2
    ld (FIFO_PORT), a
    ld a, d
    nop
    ld (FIFO_PORT), a
    ret
fifo_receive_busy:
    call fifo_status
    and #0x02
    ld e, a
    ret
fifo_transmit_busy:
    call fifo_status
    and #0x01
    ld e, a
    ret
fifo_wait_receive:
    call fifo_status
    and #0x02
    jr nz, fifo_wait_receive
    ret
fifo_wait_transmit:
    call fifo_status
    and #0x01
    jr nz, fifo_wait_transmit
    ret
fifo_status:
    ld a, #0xa0
    ld (FIFO_PORT), a
    nop
    nop
    ld a, (FIFO_PORT)
    ret
