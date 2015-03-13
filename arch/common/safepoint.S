.code64
.text

.globl record_safepoint
record_safepoint:
	pushf
	pop 64(%rdi)

	pop	%rsi
	xor %eax, %eax
	mov %rbx, (%rdi)
	mov %rsp, 8(%rdi)
	push %rsi

	mov %rbp, 16(%rdi)
	mov %r12, 24(%rdi)
	mov %r13, 32(%rdi)
	mov %r14, 40(%rdi)
	mov %r15, 48(%rdi)
	mov %rsi, 56(%rdi)

	ret

.globl interrupt_restore_safepoint
interrupt_restore_safepoint:
	movl %esi, %eax
	movq (%rdi), %rbx
	movq 8(%rdi), %rsp
	movq 16(%rdi), %rbp
	movq 24(%rdi), %r12
	movq 32(%rdi), %r13
	movq 40(%rdi), %r14
	movq 48(%rdi), %r15

	// We should return into ring0
	push $0			// SS
	push 8(%rdi)	// RSP
	push 64(%rdi)	// RFLAGS
	push $8			// CS
	push 56(%rdi)	// RIP

	iretq

.globl switch_to_ring3
switch_to_ring3:
	pop %rcx

	mov %rsp, %rax

	push $0x23
	push %rax
	pushf
	push $0x1b
	push %rcx

	iretq

.globl switch_to_ring0
switch_to_ring0:
	int $0x80

.globl int80_handler
int80_handler:
	mov 24(%rsp), %rsp
	ret
