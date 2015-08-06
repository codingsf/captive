.text

// %rdi: lhs
// %rsi: rhs
// %rdx: carry_in
// output: SZ0A0P1C0000000V
.globl genc_adc_flags
genc_adc_flags:
	movb $0xff, %al
	addb %dl, %al

	// perform add with carry
	adc %esi, %edi

	// read out flags into correct format
	lahf
	seto %al

	// return
	retq

.size genc_adc_flags, .-genc_adc_flags
