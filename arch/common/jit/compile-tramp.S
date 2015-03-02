.globl compile_basic_block
compile_basic_block:
	mov $6, %eax
	out %al, $0xff
	ret
