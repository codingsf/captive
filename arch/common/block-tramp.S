.text

.globl block_trampoline
block_trampoline:
	push %r15
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
	
	mov 0x3c(%rbp), %r15d		// Cache the PC
	callq *%rsi					// Run the Code
	mov %r15d, 0x3c(%rbp)		// Restore the PC

	// Restore the stack pointer
	mov %r13, %rsp
	pop %rbp
	pop %rbx
	pop %r12
	pop %r13
	pop %r14
	pop %r15
	retq
.size block_trampoline, .-block_trampoline

