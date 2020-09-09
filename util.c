#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sres.h"

#define ERRLEN_MAX 4096
static char err[ERRLEN_MAX+1];
static int errlen;

void *
malloc_or_exit(size_t n)
{
	void *ptr;

	if ((ptr = malloc(n)) == NULL)
		errexit("out of memory");
	return ptr;
}

void *
realloc_or_exit(void *ptr, size_t n)
{
	if ((ptr = realloc(ptr, n)) == NULL)
		errexit("out of memory");
	return ptr;
}

void
errexit(char const *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	exit(EXIT_FAILURE);
}

void
errset(char const *s)
{
	errlen = 0;
	erradd(s);
}

void
erradd(char const *s)
{
	size_t slen;

	slen = strlen(s);
	if (slen > ERRLEN_MAX - errlen - 2)
		return;
	/* Build the string backwards so we don't have to keep shifting old stuff
	 * to the right. */
	if (errlen > 0) {
		errlen += 2;
		err[ERRLEN_MAX-errlen+0] = ':';
		err[ERRLEN_MAX-errlen+1] = ' ';
	}
	errlen += slen;
	strncpy(&err[ERRLEN_MAX-errlen], s, slen);
}

char *
errget(void)
{
	return &err[ERRLEN_MAX-errlen];
}
