.text

.align 16
.globl __fast_zero_page
__fast_zero_page:
	xor %eax, %eax
	mov $0x200, %ecx
	cld
	rep stosq %rax, %es:(%rdi)
	ret

.align 16
.globl memset
memset:
	test %rsi, %rsi
	jz 1f

	cld
	mov %esi, %eax
	mov %rdx, %rcx
	jmp 2f

1:
	mov %rdx, %rsi

.align 16
.globl bzero
bzero:
	cld

	mov %rsi, %rcx
	shr $3, %rcx

	xor %eax, %eax
	
	rep stosq %rax, %es:(%rdi)

	movq %rsi, %rcx
	and $3, %rcx

.align 16
2:
	rep stosb %al, %es:(%rdi)

	mov %rdi, %rax
	ret

.align 16
.globl memcpy
memcpy:
	mov %rdi, %rax

	cld

	mov %rdx, %rcx
	shr $3, %rcx
	
	rep movsq %ds:(%rsi), %es:(%rdi)

	movq %rdx, %rcx
	and $3, %rcx

	rep movsb %ds:(%rsi), %es:(%rdi)
	
	ret
