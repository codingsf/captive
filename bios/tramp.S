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

	// Disable the PIC - we're using the LAPIC/IOAPIC
	mov $0xff, %eax
	out %al, $0xa1
	out %al, $0x21

	// Enable PAE, PGE, PSE, OSFXSR and OSXMMEXCPT bits
    mov $0x6b0, %eax
    mov %eax, %cr4

	// Setup cr3 with the PML4
    movl $0x2000, %eax
    movl %eax, %cr3

	// Enable LME and NXE
    mov $0xc0000080, %ecx
    rdmsr
    or $0x901, %eax
    wrmsr

	// Enable paging and protected mode, and the EM and MP bits
    mov %cr0, %eax
    or $0x80010007, %eax
    mov %eax, %cr0

	// Load the GDT
    lgdt %cs:gdt_ptr

	// ljmp to entry64
    .word 0xea66, entry64, 0x000f, 0x0008

.align 8
.code64
entry64:
	xorl %eax, %eax
    movw %ax, %ss

	// Clear the segment registers
	mov $0x10, %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

	// Enable SSE
	mov %cr0, %rax
	and $~(1<<2), %rax
	or $(1<<1), %rax
	mov %rax, %cr0

	mov %cr4, %rax
	or $(3<<9), %rax
	mov %rax, %cr4

	// Make a hypercall to instruct the hypervisor to restructure the
	// page tables
	movl $1, %eax
	out %al, $0xff

.align 4
gdt_ptr:
    .word 0x38
	.long 0x100

idt_ptr:
    .long 0
    .long 0

.code16
.section ".reset_vector", "ax"
reset_vector:
    jmp entry16
