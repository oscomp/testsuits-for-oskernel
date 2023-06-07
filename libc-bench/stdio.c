#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t b_stdio_putcgetc(void *dummy)
{
	FILE *f = tmpfile();
	size_t i;
	size_t cs;

	for (i=0; i<5000000; i++)
		putc('x', f);
	fseeko(f, 0, SEEK_SET);
	for (i=0; i<5000000; i++)
		cs += getc(f);
	fclose(f);

	return cs;
}

size_t b_stdio_putcgetc_unlocked(void *dummy)
{
	FILE *f = tmpfile();
	size_t i;
	size_t cs;

	for (i=0; i<5000000; i++)
		putc_unlocked('x', f);
	fseeko(f, 0, SEEK_SET);
	for (i=0; i<5000000; i++)
		cs += getc_unlocked(f);
	fclose(f);

	return cs;
}
