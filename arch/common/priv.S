.code64
.text

// Switch to KERNEL MODE
.globl int80_handler
int80_handler:
	mov %fs:0x50, %rcx		// Load pointer to _kernel_mode
	movb $1, (%rcx)			// Store 1
	movq $0x08,  8(%rsp)	// CS
	movq $0x10, 32(%rsp)	// SS

	iretq

// Switch to USER MODE
.globl int81_handler
int81_handler:
	mov %fs:0x50, %rcx		// Load pointer to _kernel_mode
	movb $0, (%rcx)			// Store 0
	movq $0x1b,  8(%rsp)	// CS
	movq $0x23, 32(%rsp)	// SS

	iretq

// Flushing the TLB
.globl int82_handler
int82_handler:
	mov $0x40090000, %eax
	mov %rax, %cr3
	iretq

.globl syscall_handler
syscall_handler:
	jmp *%rcx
