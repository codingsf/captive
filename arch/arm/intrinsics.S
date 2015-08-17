.text

// %rdi: lhs
// %rsi: rhs
// %rdx: carry_in
// output: SZ0A0P1C0000000V
.globl genc_adc_flags
genc_adc_flags:
	mov %fs:0x8, %rcx

	movb $0xff, %al
	addb %dl, %al

	// perform add with carry
	adc %esi, %edi

	// read out flags into correct format
	setc 472(%rcx)
	seto 473(%rcx)
	setz 474(%rcx)
	sets 475(%rcx)
	
	// return
	retq

.size genc_adc_flags, .-genc_adc_flags
