.globl __fast_zero_page
__fast_zero_page:
	xor %eax, %eax
	mov $0x200, %ecx
	cld
	rep stosq %rax, %es:(%rdi)
	ret

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

.globl bzero
bzero:
	cld

	mov %rsi, %rcx
	shr $3, %rcx

	xor %eax, %eax
	
	rep stosq %rax, %es:(%rdi)

	movq %rsi, %rcx
	and $3, %rcx

2:
	rep stosb %al, %es:(%rdi)

	mov %rdi, %rax
	ret
