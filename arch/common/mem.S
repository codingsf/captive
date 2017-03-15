.code64
.text

.align 16

.globl int60_handler
int60_handler:
	// If MMU disabled, nothing to do
	mov %cr3, %r14
	cmp $0x11000, %r14d
	je 1f

	//int $0x90

	// Need to keep RCX
	push %rcx

	// Grab correct table index
	mov %rcx, %r14
	shr $47, %r14
	and $3, %r14d

	// Update CR3 with correct table
	lea int60_modes, %rcx
	mov (%rcx, %r14, 8), %r14
	mov %r14, %cr3

	// Restore RCX and mask
	pop %rcx
	mov $0x7fffffffffff, %r14
	and %r14, %rcx

1:
	iretq

.globl int60_modes
int60_modes:
	.quad 0
	.quad 0
	.quad 0
	.quad 0
