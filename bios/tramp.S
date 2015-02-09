.code16gcc
.extern main

.text

.org 0

.globl entry16
entry16:
	// Clear the interrupt flag
    cli

	// Load the IDT with a null pointer
    lidt %cs:idt_ptr

	// Enable PAE and PGE
    mov $0xa0, %eax
    mov %eax, %cr4

	// Setup cr3 with the PML4
    movl $0x1000, %eax
    movl %eax, %cr3

	// Enter long mode
    mov $0xc0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

	// Enable paging and protected mode
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0

	// Load the GDT
    lgdt %cs:gdt_ptr

	// ljmp to entry64
    .word 0xea66, entry64, 0x000f, 0x0008

.align 8
.code64
entry64:
	// Clear the segment registers
    xorl %eax, %eax
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

	// Make a hypercall to instruct the hypervisor to restructure the
	// page tables
	movl $1, %eax
	out %al, $0xff

.align 4
gdt_ptr:
    .word 0x18
    .word gdt
    .word 0x000f
    .word 0
	.word 0

.align 16
gdt:
    .quad 0x0000000000000000		// NULL descriptor
    .quad 0x0020980000000000		// Code descriptor
    .quad 0x0000900000000000		// Data descriptor

idt_ptr:
    .long 0
    .long 0

.code16
.section ".reset_vector", "ax"
reset_vector:
    jmp entry16
