.code16gcc
.extern main
.text

.org 0

.globl entry16
entry16:
    cli

    lidt %cs:idt_ptr

    mov $0xa0, %eax
    mov %eax, %cr4

.extern pgd
    movl $0xf1000, %eax
    //addl pgd, %eax
    movl %eax, %cr3

    mov $0xc0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0

    lgdt %cs:gdt_ptr

    .word 0xea66, entry64, 0x000f, 0x0008

    nop
    nop

.code64
entry64:
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    mov %ax, %ss

    xorl %eax, %eax
    movw %ax, %fs
    movw %ax, %gs

    jmp main

.code16
.align 4
gdt_ptr:
    .word 0x18
    .word gdt
    .word 0x000f
    .word 0

.align 16
gdt:
    .quad 0x0000000000000000
    .quad 0x0020980000000000
    .quad 0x0000900000000000

idt_ptr:
    .word 0
    .long 0
    .word 0

.section ".reset_vector", "ax"
reset_vector:
    jmp entry16
