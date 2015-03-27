.text

.globl spin_lock
spin_lock:
	mov $1, %eax
	xchgq (%rdi), %rax
	test %eax, %eax
	jnz spin_lock
	ret

.globl spin_unlock
spin_unlock:
	mov $0, %eax
	xchgq (%rdi), %rax

	ret
