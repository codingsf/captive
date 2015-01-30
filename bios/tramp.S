.code16gcc
.extern main
.text

.org 0
nop

.globl entry16
entry16:
    cli
    lgdt %cs:gdt_ptr

    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0

    .word 0xea66, entry32, 0x000f, 0x0010

    nop

.code32
entry32:
    movl $0x18, %eax
    movw %ax, %ds
    movw %ax, %es
    mov %ax, %ss

    xorl %eax, %eax
    movw %ax, %fs
    movw %ax, %gs

    cld

    jmp main

.code16
gdt_ptr:
    .word 0x30
    .word gdt
    .word 0x000f

gdt:
    .word 0, 0, 0, 0
    .word 0, 0, 0, 0
    .word 0xffff, 0, 0x9b00, 0x00cf // 32 bit flat code segment (0x10)
    .word 0xffff, 0, 0x9300, 0x00cf // 32 bit flat data segment (0x18)
    .word 0xffff, 0, 0x9b0f, 0x0000 // 16 bit code segment base=0xf0000 limit=0xffff
    .word 0xffff, 0, 0x9300, 0x0000 // 16 bit data segment base=0x0 limit=0xffff

.section ".reset_vector", "ax"
reset_vector:
    jmp entry16
