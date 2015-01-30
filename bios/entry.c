asm (".code64\n");

unsigned long int pgd[4] __attribute__((aligned(0x1000)))= {0xf0003};

void main()
{
	volatile int *x = (volatile int *)0xa0b0c0d0;
	int y = *x;

	for(;;);
}
