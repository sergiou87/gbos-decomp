.module entry

.globl boot_start
.globl reset_vector
.globl cartridge_entry

.area ENTRY (ABS)
.org 0x0000
reset_vector:
    jp boot_start

.org 0x0100
cartridge_entry:
    jp boot_start
