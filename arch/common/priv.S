.code64
.text

// Switching to ring3 from ring0
.globl int80_handler
int80_handler:
	movq $0x08,  8(%rsp)		// CS
	movq $0x10, 32(%rsp)		// SS

	iretq

// Switching to ring0 from ring3
.globl int81_handler
int81_handler:
	movq $0x1b,  8(%rsp)		// CS
	movq $0x23, 32(%rsp)		// SS

	iretq

// Flushing the TLB
.globl int82_handler
int82_handler:
	mov $0x40010000, %eax
	mov %rax, %cr3
	iretq
