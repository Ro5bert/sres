
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sres.h"

void
errexit(char const *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	exit(1);
}

void *
malloc_or_exit(size_t n)
{
	void *ptr;

	if ((ptr = malloc(n)) == NULL) errexit("out of memory");
	return ptr;
}

void *
realloc_or_exit(void *ptr, size_t n)
{
	if ((ptr = realloc(ptr, n)) == NULL) errexit("out of memory");
	return ptr;
}

#define ERRCTX_LEN 16
static char const *errctx[ERRCTX_LEN];
static int errctxi = 0;
#define ERRCTX_LINECNT_LEN 32
static char errctx_linecnt[ERRCTX_LINECNT_LEN];

void
push_errctx(char const *msg)
{
	if (errctxi < ERRCTX_LEN) errctx[errctxi++] = msg;
}

void
push_errctx_linecnt(long linecnt)
{
	snprintf(errctx_linecnt, ERRCTX_LINECNT_LEN, "line %ld", linecnt);
	push_errctx(errctx_linecnt);
}

char *
concat_errctx(void)
{
	char *s;
	size_t slen, offs, need;

	for (s = NULL, offs = 0, slen = 64; errctxi; --errctxi) {
		need = strlen(errctx[errctxi-1])+2;
		while (need > slen - offs) {
			s = realloc_or_exit(s, slen *= 2);
		}
		memcpy(&s[offs], errctx[errctxi-1], need-2);
		offs += need-2;
		if (errctxi > 1) {
			s[offs++] = ':';
			s[offs++] = ' ';
		} else {
			s[offs++] = '\0';
		}
	}

	return s;
}
