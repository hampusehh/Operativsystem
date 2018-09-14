.section .text
.global _start
_start:
    call  main
    xorl  %edx, %edx
    int   $0x80
.size _start, . - _start
