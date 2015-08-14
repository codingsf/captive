.text

.globl block_trampoline
block_trampoline:
	push %r15
	push %r14
	push %r13
	push %r12
	push %rbx
	push %rbp
	
	mov %rsp, %rbp
	sub $0x40, %rsp
	mov %fs:0x8, %rdi
	callq *%rsi
	
	leaveq
	pop %rbx
	pop %r12
	pop %r13
	pop %r14
	pop %r15
	retq
.size block_trampoline, .-block_trampoline

