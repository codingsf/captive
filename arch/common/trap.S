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
//trap_macro pf,1
trap_macro gpf,1
trap_macro irq,0

.global trap_pf
.extern interrupt_restore_safepoint
.extern cpu_safepoint

trap_pf:
	push_regs

	mov %cr2, %rdi
	movq (8 * 16)(%rsp), %rsi
	movq (8 * 17)(%rsp), %rdx

	call handle_pagefault

	test %rax, %rax
	jz 1f

	leaq cpu_safepoint, %rdi
	mov %rax, %rsi
	jmp interrupt_restore_safepoint

1:
	pop_regs
	add $8, %rsp
	iretq
