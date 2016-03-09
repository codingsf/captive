.text

.global prebuilt_11000
.global prebuilt_11010

prebuilt_11000:
	movl   $0xc200,0x8(%rdi)
	addl   $0x4,0x3c(%rdi)

	movl   $0x0,0xc(%rdi)
	addl   $0x4,0x3c(%rdi)

	mov    0x8(%rdi),%eax
	movzx  %ax, %eax
	or     $0xbeb0000,%eax
	mov    %eax,0x8(%rdi)
	addl   $0x4,0x3c(%rdi)

	mov    0xc(%rdi),%eax
	movzx  %ax, %eax
	or     $0x210000,%eax
	mov    %eax,0xc(%rdi)
	addl   $0x4,0x3c(%rdi)

prebuilt_11010:
	mov    0xc(%rdi),%edx
	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	subl    $0x1,0x8(%rdi)
	setae  0x140(%rdi)
	seto   0x143(%rdi)
	sete   0x141(%rdi)
	sets   0x142(%rdi)

// -----
	mov    %eax,(%rdx)				// Store EBX into (EDX)  r1 => [r3]

	mov    (%rdx),%eax				// Load (EDX) into EAX	[r3]
	mov    %eax,0x4(%rdi)			// Store EAX into r1

	mov    %eax,(%rdx)				// Store EAX into (EDX)

	mov    (%rdx),%eax				// Load [r3] into EAX
	mov    %eax,0x4(%rdi)			// Store EAX into [r1]

	mov    %eax,(%rdx)				// Store EAX into [r3]

	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	mov    %eax,(%rdx)

	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	mov    %eax,(%rdx)

	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	mov    %eax,(%rdx)

	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	mov    %eax,(%rdx)

	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	mov    %eax,(%rdx)

	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	mov    %eax,(%rdx)

	mov    (%rdx),%eax
	mov    %eax,0x4(%rdi)

	mov    %eax,(%rdx)
	addl   $0x54,0x3c(%rdi)

// -----

	cmpb   $0,0x141(%rdi)
	jz    1f

	addl   $0x4,0x3c(%rdi)
	jmp 2f

1:
	subl    $0x54,0x3c(%rdi)

2:
	mov    %fs:0x30,%eax
	test   %eax,%eax
	jne    3f

	cmpl $0x11010, 0x3c(%rdi)
	je prebuilt_11010

	mov    0x3c(%rdi),%eax
	mov    %eax,%ebx
	shr    $0x2,%eax
	movzwl %ax,%edx
	shl    $0x4,%edx
	mov    %fs:0x20,%rcx

	add    %rcx,%rdx
	cmp    %ebx,(%rdx)

	jne    3f
	jmpq   *0x8(%rdx)

3:
	xor    %eax,%eax
	retq

/*
prebuilt_11000:
	.incbin "/disk/scratch/spink/captive/arch/common/prebuilt/p-00011000.bin"
* /

prebuilt_11010:
	.incbin "/disk/scratch/spink/captive/arch/common/prebuilt/p-00011010.bin"

*/
