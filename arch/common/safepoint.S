.code64
.text

.globl record_safepoint
record_safepoint:
	pushf
	pop 64(%rdi)

	movq %rbx, (%rdi)
	pop %rcx
	movq %rsp, 8(%rdi)
	push %rcx
	movq %rbp, 16(%rdi)
	movq %r12, 24(%rdi)
	movq %r13, 32(%rdi)
	movq %r14, 40(%rdi)
	movq %r15, 48(%rdi)
	movq %rcx, 56(%rdi)

	xor %eax, %eax
	ret

.globl restore_safepoint
restore_safepoint:
	movq (%rdi), %rbx
	movq 8(%rdi), %rsp
	movq 16(%rdi), %rbp
	movq 24(%rdi), %r12
	movq 32(%rdi), %r13
	movq 40(%rdi), %r14
	movq 48(%rdi), %r15

	push 64(%rdi)
	popf

	mov %rsi, %rax

	push 56(%rdi)	// RIP
	retq

.globl interrupt_restore_safepoint
interrupt_restore_safepoint:
	// RAX should be set on entry to this

	movq (%rdi), %rbx
	movq 8(%rdi), %rsp
	movq 16(%rdi), %rbp
	movq 24(%rdi), %r12
	movq 32(%rdi), %r13
	movq 40(%rdi), %r14
	movq 48(%rdi), %r15

	// We should return into ring0
	push $0x10		// SS
	push 8(%rdi)	// RSP
	push 64(%rdi)	// RFLAGS
	push $0x08		// CS
	push 56(%rdi)	// RIP

	iretq

.globl interrupt_restore_trampoline
.extern block_trampoline
interrupt_restore_trampoline:
	// RDI - safepoint address
	// RSI - translated code address
	// RAX - return value

	movq (%rdi), %rbx
	movq 8(%rdi), %rsp
	movq 16(%rdi), %rbp
	movq 24(%rdi), %r12
	movq 32(%rdi), %r13
	movq 40(%rdi), %r14
	movq 48(%rdi), %r15

	// Push the safepoint address on to the stack here
	push 56(%rdi)	// RIP

	// We should return into ring0
	push $0x10		// SS
	push 8(%rdi)	// RSP
	push 64(%rdi)	// RFLAGS
	push $0x08		// CS

	// Now, push the block trampoline address here, so that we return directly
	// into the trampoline
	push block_trampoline

	iretq
