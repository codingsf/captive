.code64
.text

.macro push_regs
	pushf
	push %rax
	push %rbx
	push %rcx
	push %rdx
	push %rdi
	push %rsi
	push %rbp
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15
.endm

.macro pop_regs
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rbp
	pop %rsi
	pop %rdi
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
	popf
.endm

// Syscall Handler
.globl int82_handler
.extern do_syscall
int82_handler:
	push $0
	push_regs
	mov %rsi, %rdx
	mov %rdi, %rsi
	mov %rsp, %rdi
	call do_syscall
	pop_regs
	add $8, %rsp
	iretq

// JIT Syscall Handler
.globl int85_handler
.extern do_syscall
int85_handler:
	push $0
	push_regs
	mov %rsp, %rdi
	mov %rbx, %rsi
	mov %rcx, %rdx
	call do_syscall
	pop_regs
	add $8, %rsp
	iretq

.globl tail_call_ret0_only
tail_call_ret0_only:
	xor %eax, %eax
	ret
