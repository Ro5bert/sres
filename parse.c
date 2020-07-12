
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "sres.h"

static char *
nexttok(char **s, char delim)
{
	char *tok;

	tok = *s;
	if (*s) {
		if ((*s = strchr(*s, delim)) != NULL) {
			if (delim != '\0') {
				**s = '\0';
				++*s;
			} else {
				*s = NULL;
			}
		}
	}
	return tok;
}

static void
skipws(char **s) {
	while (isspace(**s)) {
		++*s;
	}
}

static void
stripws(char **s)
{
	char *end;

	skipws(s);
	end = strchr(*s, '\0');
	if (end != *s) { /* in case *s == '\0' upon invocation */
		while (isspace(*--end)) {
			*end = '\0';
		}
	}
}

static int
parse_minutes(char **s)
{
	int m;

	if (!isdigit((*s)[0]) || !isdigit((*s)[1])) {
		push_errctx("bad minutes: invalid length or contains non digits");
		return -1;
	}
	m = ((*s)[0] - '0') * 10 + ((*s)[1] - '0');
	if (m < 0 || m >= 60) {
		push_errctx("bad minutes: out of bounds (need 0 <= m < 60)");
		return -1;
	}
	*s += 2;

	return m;
}

static int
parse_hours(char **s)
{
	int h;

	if (!isdigit((*s)[0]) || !isdigit((*s)[1])) {
		push_errctx("bad hours: invalid length or contains non digits");
		return -1;
	}
	h = ((*s)[0] - '0') * 10 + ((*s)[1] - '0');
	if (h < 0 || h >= 24) {
		push_errctx("bad hours: out of bounds (need 0 <= h < 24)");
		return -1;
	}
	*s += 2;

	return h;
}

static int
parse_dow(char **s)
{
	int dow;
	char b[2];

	if ((*s)[0] == '\0' || (*s)[1] == '\0') {
		push_errctx("bad day of week: invalid length");
		return -1;
	}
	b[0] = tolower((*s)[0]);
	b[1] = tolower((*s)[1]);
	if      (b[0]=='s' && b[1]=='u') dow = SU;
	else if (b[0]=='m' && b[1]=='o') dow = MO;
	else if (b[0]=='t' && b[1]=='u') dow = TU;
	else if (b[0]=='w' && b[1]=='e') dow = WE;
	else if (b[0]=='t' && b[1]=='h') dow = TH;
	else if (b[0]=='f' && b[1]=='r') dow = FR;
	else if (b[0]=='s' && b[1]=='a') dow = SA;
	else {
		push_errctx("bad day of week");
		return -1;
	}
	*s += 2;

	return dow;
}

static int
parse_dom(char **s)
{
	int dom;

	if (!isdigit((*s)[0]) || !isdigit((*s)[1])) {
		push_errctx("bad day of month: invalid length or contains non digits");
		return -1;
	}
	dom = ((*s)[0] - '0') * 10 + ((*s)[1] - '0');
	if (dom < 1 || dom > 31) {
		push_errctx("bad day of month: out of bounds (need 1 <= dom <= 31)");
		return -1;
	}
	dom -= 1; /* Make dom start at 0 like every other contraint. */
	*s += 2;

	return dom;
}

static int
parse_month(char **s)
{
	int m;
	char b[3];

	if ((*s)[0] == '\0' || (*s)[1] == '\0' || (*s)[2] == '\0') {
		push_errctx("bad month: invalid length");
		return -1;
	}
	b[0] = tolower((*s)[0]);
	b[1] = tolower((*s)[1]);
	b[2] = tolower((*s)[2]);
	if      (b[0]=='j' && b[1]=='a' && b[2]=='n') m = JAN;
	else if (b[0]=='f' && b[1]=='e' && b[2]=='b') m = FEB;
	else if (b[0]=='m' && b[1]=='a' && b[2]=='r') m = MAR;
	else if (b[0]=='a' && b[1]=='p' && b[2]=='r') m = APR;
	else if (b[0]=='m' && b[1]=='a' && b[2]=='y') m = MAY;
	else if (b[0]=='j' && b[1]=='u' && b[2]=='n') m = JUN;
	else if (b[0]=='j' && b[1]=='u' && b[2]=='l') m = JUL;
	else if (b[0]=='a' && b[1]=='u' && b[2]=='g') m = AUG;
	else if (b[0]=='s' && b[1]=='e' && b[2]=='p') m = SEP;
	else if (b[0]=='o' && b[1]=='c' && b[2]=='t') m = OCT;
	else if (b[0]=='n' && b[1]=='o' && b[2]=='v') m = NOV;
	else if (b[0]=='d' && b[1]=='e' && b[2]=='c') m = DEC;
	else {
		push_errctx("bad month");
		return -1;
	}
	*s += 3;

	return m;
}

static int
parse_year(char **s)
{
	int y;

	/* Need exactly 4 digits, since we only allow years after the Unix Epoch and
	 * before 2038 (ABIs with 32-bit time_t will break) (this restriction can
	 * be lifted in the future). */
	if (!isdigit((*s)[0]) || !isdigit((*s)[1]) ||
			!isdigit((*s)[2]) || !isdigit((*s)[3])) {
		push_errctx("bad year: invalid length or contains non digits");
		return -1;
	}
	y = ((*s)[0] - '0') * 1000 +
		((*s)[1] - '0') *  100 +
		((*s)[2] - '0') *   10 +
		((*s)[3] - '0');
	if (y < 1970 || y >= 2038) {
		push_errctx("bad year: out of bounds (need 1970 <= y < 2038)");
		return -1;
	}
	*s += 4;

	return y;
}

static int
parse_constraint(
		struct constraint **arr, int *len,
		char *s, int (*str2num)(char**))
{
	char *tok1, *tok2;
	int cap;

	if (strchr(s, '*')) {
		if (strlen(s) > 1) {
			push_errctx("invalid use of wildcard");
			return -1;
		}
		*arr = malloc_or_exit(sizeof **arr);
		*len = 1;
		(*arr)[0].begin = (*arr)[0].end = -1;
	} else {
		*arr = malloc_or_exit(4 * sizeof **arr);
		cap = 4;
		for (*len = 0; s; ++*len) {
			if (*len == cap) {
				*arr = realloc_or_exit(*arr, cap *= 2);
			}
			tok2 = nexttok(&s, ',');
			tok1 = nexttok(&tok2, '-');
			if (((*arr)[*len].begin = str2num(&tok1)) < 0) return -1;
			if (tok2) { /* The constraint is a range. */
				if (((*arr)[*len].end = str2num(&tok2)) < 0) return -1;
				if ((*arr)[*len].end <= (*arr)[*len].begin) {
					push_errctx("invalid range");
					return -1;
				}
			} else { /* The constraint is a single value. */
				(*arr)[*len].end = (*arr)[*len].begin;
			}
		}
	}

	return 0;
}

static int
parse_duration(long *dur, char *s)
{
	char last;
	long sign, l, scale;

	*dur = 0;
	sign = 1;
	l = 0;
	scale = 1;
	for (last = '\0'; s[0] != '\0'; ++s) {
		switch (last = s[0]) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (l > (LONG_MAX - (s[0]-'0')) / 10) {
				push_errctx("bad duration: out of bounds");
				return -1;
			}
			l = l*10 + (s[0]-'0');
			break;
		case 'w': scale *= 7;
		case 'd': scale *= 24;
		case 'h': scale *= 60;
		case 'm':
			if (l > (LONG_MAX) / scale) {
				push_errctx("bad duration: out of bounds");
				return -1;
			}
			l *= sign * scale;
			if ((l > 0 && *dur > LONG_MAX - l) ||
					(l < 0 && *dur < LONG_MIN - l)) {
				push_errctx("bad duration: out of bounds");
				return -1;
			}
			*dur += l;
			sign = 1;
			l = 0;
			scale = 1;
			break;
		case '-':
			if (sign == 1) {
				sign = -1;
				break;
			}
			/* FALLTHROUGH */
		default:
			push_errctx("bad duration: invalid character");
			return -1;
		}
	}
	/* last == '\0' is OK: it means empty duration. */
	if (!strchr("wdhm", last)) {
		push_errctx("bad duration: missing unit");
		return -1;
	}

	return 0;
}

static int
parse_constraints(struct entry *entry, char *s)
{
	char *tok;

	++s; /* skip over '@' */

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_constraint(&entry->min, &entry->minl, tok, parse_minutes) < 0)
		return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_constraint(&entry->hour, &entry->hourl, tok, parse_hours) < 0)
		return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_constraint(&entry->dow, &entry->dowl, tok, parse_dow) < 0)
		return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_constraint(&entry->dom, &entry->doml, tok, parse_dom) < 0)
		return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_constraint(&entry->month, &entry->monthl, tok, parse_month) < 0)
		return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_constraint(&entry->year, &entry->yearl, tok, parse_year) < 0)
		return -1;

	skipws(&s);
	tok = nexttok(&s, '\0');
	if (parse_duration(&entry->dur, tok) < 0) return -1;
	if (entry->dur < 0) {
		push_errctx("bad duration: must be nonnegative");
		return -1;
	}

	return 0;
}

int
parse_entries(struct entry **entry)
{
	char *line, *s;
	size_t len;
	long linecnt;

	*entry = NULL;
	line = NULL;
	len = 0;
	linecnt = 0;
	while (getline(&line, &len, stdin) != -1) {
		assert(len > 0);
		++linecnt;
		s = line;
		stripws(&s);
		if (s[0] == '#' || s[0] == '\0') {
			/* Ignore line comments and empty lines. */
			continue;
		}
		if (s[0] != '@') { /* new entry */
			if (*entry != NULL) {
				/* This happens for every entry except the first. */
				entry = &(*entry)->next;
			}
			*entry = malloc_or_exit(sizeof **entry);
			(*entry)->next = NULL;
			(*entry)->dur = -1;
			(*entry)->text = malloc_or_exit(strlen(s)+1);
			strcpy((*entry)->text, s);
		} else { /* selector for prev entry */
			if (*entry == NULL) {
				push_errctx("selector without text");
				push_errctx_linecnt(linecnt);
				free(line);
				return -1;
			}
			/* dur >= 0 means the entry is already populated, so we need to
			 * allocate a new one. */
			if ((*entry)->dur >= 0) {
				(*entry)->next = malloc_or_exit(sizeof **entry);
				(*entry)->next->next = NULL;
				(*entry)->next->text = (*entry)->text;
				entry = &(*entry)->next;
			}
			if (parse_constraints(*entry, s) < 0) {
				push_errctx_linecnt(linecnt);
				free(line);
				return -1;
			}
		}
	}

	free(line);
	return 0;
}

/* TODO make parse_xxx return boolean values instead of -1 */