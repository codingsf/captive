.code64
.text

// Switching to ring0 from ring3
.globl int80_handler
int80_handler:
	movq $0x08,  8(%rsp)		// CS
	movq $0x10, 32(%rsp)		// SS

	iretq

// Switching to ring3 from ring0
.globl int81_handler
int81_handler:
	movq $0x1b,  8(%rsp)		// CS
	movq $0x23, 32(%rsp)		// SS

	iretq

.globl spin_lock
spin_lock:
	cli

	mov $1, %eax
	xchgq (%rdi), %rax
	test %eax, %eax
	jnz spin_lock
	ret

.globl spin_unlock
spin_unlock:
	mov $0, %eax
	xchgq (%rdi), %rax
	sti
	ret
