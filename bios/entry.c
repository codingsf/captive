/*
 * entry.c
 */
void main()
{
	volatile int *x = (volatile int *)0xa0b0c0d0;
	int y = *x;

	for(;;);
}