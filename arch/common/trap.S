.code64
.text

.globl start
.extern start_environment
start:
	movq $0xdeadbabecafef00d, %rax
	push %rax
	push %rax

	jmp start_environment

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

.macro enter_intr,has_arg
.if ! \has_arg
	push $0
.endif
	push_regs

	mov %rsp, %rdi
.endm

.macro exit_intr
	pop_regs
	add $8, %rsp
	iretq
.endm

.macro trap_macro name,has_arg
.extern handle_trap_\name
.globl trap_\name
trap_\name:
	enter_intr \has_arg
	call handle_trap_\name
	exit_intr
.endm

trap_macro unk,0
trap_macro unk_arg,1
trap_macro gpf,1

.global trap_pf
.extern interrupt_restore_safepoint
.extern cpu_safepoint
.extern handle_pagefault

trap_pf:
	enter_intr 1

	mov %cr2, %rsi
	call handle_pagefault

	test %rax, %rax
	jnz 1f
	exit_intr

1:
	leaq cpu_safepoint, %rdi
	jmp interrupt_restore_safepoint

.globl trap_signal
.extern
trap_signal:
	enter_intr 0
	call handle_signal
	exit_intr
