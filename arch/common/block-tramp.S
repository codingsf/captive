.text
.align 16
.globl block_trampoline
block_trampoline:
	push %r14
	push %r13
	push %r12
	push %rbx
	push %rbp
	
	// Store the frame pointer, and allocate temporary stack storage
	mov %rsp, %r13
	sub $0x40, %rsp

	// Load the register file base address
	mov %fs:0x8, %rbp

	// Load the event loop base address
	mov %fs:0x48, %rax
	mov 0x10(%rax), %r12
	
	callq *%rsi					// Run the Code

	// Restore the stack pointer
	mov %r13, %rsp
	pop %rbp
	pop %rbx
	pop %r12
	pop %r13
	pop %r14
	retq
.size block_trampoline, .-block_trampoline

