.code64

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

.macro trap_macro name,has_arg
.extern handle_trap_\name
.globl trap_\name
trap_\name:
	push_regs

	movq (8 * (16 + \has_arg))(%rsp), %rdi
.if \has_arg
	movq (8 * 16)(%rsp), %rsi
.endif
	call handle_trap_\name

	pop_regs

.if \has_arg
	add $8, %rsp
.endif

	iretq
.endm

trap_macro unk,0
trap_macro dbz,0
trap_macro dbg,0
trap_macro nmi,0
trap_macro pf,1
trap_macro gpf,1
