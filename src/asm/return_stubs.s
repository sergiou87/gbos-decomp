.module return_stubs

.area RETURN_STUBS (ABS)
.org 0x0008
restart_08:
    reti
.org 0x0010
restart_10:
    reti
.org 0x0018
restart_18:
    reti
.org 0x0020
restart_20:
    reti
.org 0x0028
restart_28:
    reti
.org 0x0030
restart_30:
    reti
.org 0x0038
restart_38:
    reti

; Offset 0x0066 is not a Game Boy interrupt vector.
.org 0x0066
unused_interrupt_return:
    reti

; No callers or original names have been established for these return stubs.
.org 0x091a
unused_return_first:
    ret
unused_return_second:
    ret
unused_return_third:
    ret
