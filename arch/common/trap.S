.code64

.macro push_regs
/*	push %rax
	push %rbx
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %rbp
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15*/
	pushf
	push %rax
	push %rcx
	push %rdx
	push %r8
	push %r9
	push %r10
	push %r11
.endm

.macro pop_regs
/*	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rbp
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax*/
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdx
	pop %rcx
	pop %rax
	popf
.endm

.macro trap_macro x
.extern handle_trap_\x
.globl trap_\x
trap_\x:
	push_regs
	call handle_trap_\x
	pop_regs
	iretq
.endm

.macro trap_macro_arg x
.extern handle_trap_\x
.globl trap_\x
trap_\x:
	push_regs

	movq 128(%rsp), %rdi
	movq 120(%rsp), %rsi
	call handle_trap_\x

	pop_regs
	add $8, %rsp

// TODO: FIX THIS TERRIBLE HACK
//	add $3, (%rsp)
	iretq
.endm

trap_macro unk
trap_macro dbz
trap_macro dbg
trap_macro nmi
trap_macro_arg pf
trap_macro_arg gpf
