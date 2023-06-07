#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <langinfo.h>

size_t b_utf8_bigbuf(void *dummy)
{
	char *buf;
	wchar_t *wbuf;
	size_t i, j, k, l;
	size_t cs;

	setlocale(LC_CTYPE, "C.UTF-8")
	|| setlocale(LC_CTYPE, "en_US.UTF-8")
	|| setlocale(LC_CTYPE, "en_GB.UTF-8")
	|| setlocale(LC_CTYPE, "en.UTF-8")
	|| setlocale(LC_CTYPE, "de_DE-8")
	|| setlocale(LC_CTYPE, "fr_FR-8");
	if (strcmp(nl_langinfo(CODESET), "UTF-8")) return -1;

	buf = malloc(500000);
	wbuf = malloc(500000*sizeof(wchar_t));
	l = 0;
	for (i=0xc3; i<0xe0; i++)
		for (j=0x80; j<0xc0; j++)
			buf[l++] = i, buf[l++] = j;
	for (i=0xe1; i<0xed; i++)
		for (j=0x80; j<0xc0; j++)
			for (k=0x80; k<0xc0; k++)
				buf[l++] = i, buf[l++] = j, buf[l++] = k;
	for (i=0xf1; i<0xf4; i++)
		for (j=0x80; j<0xc0; j++)
			for (k=0x80; k<0xc0; k++)
				buf[l++] = i, buf[l++] = j, buf[l++] = 0x80, buf[l++] = k;
	buf[l++] = 0;
	for (i=0; i<50; i++)
		cs += mbstowcs(wbuf, buf, 500000);
	free(wbuf);
	free(buf);
	return cs;
}

size_t b_utf8_onebyone(void *dummy)
{
	char *buf;
	wchar_t wc;
	size_t i, j, k, l;
	size_t cs;
	mbstate_t st = {0};

	setlocale(LC_CTYPE, "C.UTF-8")
	|| setlocale(LC_CTYPE, "en_US.UTF-8")
	|| setlocale(LC_CTYPE, "en_GB.UTF-8")
	|| setlocale(LC_CTYPE, "en.UTF-8")
	|| setlocale(LC_CTYPE, "de_DE-8")
	|| setlocale(LC_CTYPE, "fr_FR-8");
	if (strcmp(nl_langinfo(CODESET), "UTF-8")) return -1;

	buf = malloc(500000);
	l = 0;
	for (i=0xc3; i<0xe0; i++)
		for (j=0x80; j<0xc0; j++)
			buf[l++] = i, buf[l++] = j;
	for (i=0xe1; i<0xed; i++)
		for (j=0x80; j<0xc0; j++)
			for (k=0x80; k<0xc0; k++)
				buf[l++] = i, buf[l++] = j, buf[l++] = k;
	for (i=0xf1; i<0xf4; i++)
		for (j=0x80; j<0xc0; j++)
			for (k=0x80; k<0xc0; k++)
				buf[l++] = i, buf[l++] = j, buf[l++] = 0x80, buf[l++] = k;
	buf[l++] = 0;
	for (i=0; i<50; i++) {
		for (j=0; buf[j]; j+=mbrtowc(&wc, buf+j, 4, &st));
	}
	free(buf);
	return cs;
}
