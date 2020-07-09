
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

enum dow {
	SU, MO, TU, WE, TH, FR, SA
};

enum month {
	JAN, FEB, MAR, APR, MAY, JUN,
	JUL, AUG, SEP, OCT, NOV, DEC
};

enum sel_constraint_type {
	SEL_AT, SEL_SPAN, SEL_WILD
};

struct sel_constraint {
	enum sel_constraint_type type;
	union {
		int at;
		struct {
			int begin, end;
		} span;
	};
	struct sel_constraint *next;
};

struct selector {
	struct sel_constraint min, hour, dow, dom, month, year;
	long dur;
	struct selector *next;
};

struct entry {
	char *text;
	struct selector *sel;
	struct entry *next;
};

struct entry_iter {
	struct entry *entry;
};

void
errexit(char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	exit(1);
}

void *
safemalloc(size_t n)
{
	void *ptr;

	if ((ptr = malloc(n)) == NULL) {
		errexit("out of memory");
	}
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
concat_errctx()
{
	char *s;
	size_t slen, offs, need;

	for (s = NULL, offs = 0, slen = 64; errctxi; slen *= 2) {
		if (!(s = realloc(s, slen))) errexit("out of memory");
		for (; errctxi; --errctxi) {
			need = strlen(errctx[errctxi-1])+2;
			if (need > slen - offs) break; /* Double buffer size. */
			memcpy(&s[offs], errctx[errctxi-1], need-2);
			offs += need-2;
			if (errctxi > 1) {
				s[offs++] = ':';
				s[offs++] = ' ';
			} else {
				s[offs++] = '\0';
			}
		}
	}

	return s;
}

char *
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

void
skipws(char **s) {
	while (isspace(**s)) {
		++*s;
	}
}

void
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

int
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

int
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

int
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

int
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
	*s += 2;

	return dom;
}

int
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

int
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

int
parse_sel_constraint(struct sel_constraint *head, char *s, int (*str2num)(char**))
{
	char *tok1, *tok2;
	struct sel_constraint **sc;

	head->next = NULL;
	sc = &head;
	if (strchr(s, '*')) {
		if (strlen(s) > 1) {
			push_errctx("invalid use of wildcard");
			return -1;
		}
		(*sc)->type = SEL_WILD;
	} else {
		while (s) {
			if (*sc == NULL) {
				*sc = safemalloc(sizeof **sc);
				(*sc)->next = NULL;
			}
			tok2 = nexttok(&s, ',');
			tok1 = nexttok(&tok2, '-');
			if (tok2) {
				(*sc)->type = SEL_SPAN;
				if (((*sc)->span.begin = str2num(&tok1)) < 0) return -1;
				if (((*sc)->span.end = str2num(&tok2)) < 0) return -1;
				if ((*sc)->span.end <= (*sc)->span.begin) {
					push_errctx("invalid range");
					return -1;
				}
			} else {
				(*sc)->type = SEL_AT;
				if (((*sc)->at = str2num(&tok1)) < 0) return -1;
			}
			sc = &(*sc)->next;
		}
	}

	return 0;
}

int
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

int
parse_selector(struct selector *sel, char *s)
{
	char *tok;

	++s; /* skip over '@' */

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_sel_constraint(&sel->min, tok, parse_minutes) < 0) return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_sel_constraint(&sel->hour, tok, parse_hours) < 0) return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_sel_constraint(&sel->dow, tok, parse_dow) < 0) return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_sel_constraint(&sel->dom, tok, parse_dom) < 0) return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_sel_constraint(&sel->month, tok, parse_month) < 0) return -1;

	skipws(&s);
	tok = nexttok(&s, ' ');
	if (parse_sel_constraint(&sel->year, tok, parse_year) < 0) return -1;

	skipws(&s);
	tok = nexttok(&s, '\0');
	if (parse_duration(&sel->dur, tok) < 0) return -1;
	if (sel->dur < 0) {
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
	struct selector **sel;

	*entry = NULL;
	line = NULL;
	len = 0;
	linecnt = 0;
	sel = NULL;
	while (getline(&line, &len, stdin) != -1) {
		assert(len > 0);
		++linecnt;
		s = line;
		stripws(&s);
		if (s[0] == '#' || s[0] == '\0') {
			/* ignore line comment and empty line */
			continue;
		}
		if (s[0] != '@') { /* new entry */
			*entry = safemalloc(sizeof **entry);
			(*entry)->sel = NULL;
			(*entry)->next = NULL;
			(*entry)->text = safemalloc(strlen(s)+1);
			strcpy((*entry)->text, s);
			sel = &(*entry)->sel;
			entry = &(*entry)->next;
		} else { /* selector for prev entry */
			if (sel == NULL) {
				push_errctx("selector without title");
				push_errctx_linecnt(linecnt);
				free(line);
				return -1;
			}
			*sel = safemalloc(sizeof **sel);
			(*sel)->next = NULL;
			if (parse_selector(*sel, s) < 0) {
				push_errctx_linecnt(linecnt);
				free(line);
				return -1;
			}
			sel = &(*sel)->next;
		}
	}

	free(line);
	return 0;
}

/* time_t */
/* parse_instant(char *s) */
/* { */
	/* struct tm tm; */
	/* int mins, sign; */
	/* long dur; */

	/* tm.tm_sec = 0; */
	/* mins = time2num(&s); */
	/* tm.tm_min = mins % 60; */
	/* tm.tm_hour = mins / 60; */
	/* tm.tm_mday = dom2num(&s); */
	/* tm.tm_mon = month2num(&s); */
	/* tm.tm_year = year2num(&s); */
	/* [> tm_wday and tm_yday ignored by mktime. <] */
	/* [> Negative isdst means mktime figures it out. <] */
	/* tm.tm_isdst = -1; */

	/* if (s[0] != '\0') { */
		/* if      (s[0] == '+') sign = 1; */
		/* else if (s[0] == '-') sign = -1; */
		/* else errexit(""); */
		/* dur = parse_duration(s+1); */
	/* } */

	/* return mktime(&tm); */
/* } */

void
print_constraint(struct sel_constraint *sc)
{
	while (sc) {
		switch (sc->type) {
		case SEL_WILD:
			printf("*");
			break;
		case SEL_AT:
			printf("%d", sc->at);
			break;
		case SEL_SPAN:
			printf("%d-%d", sc->span.begin, sc->span.end);
			break;
		}
		if ((sc = sc->next)) {
			printf(",");
		}
	}
}

void
print_entries(struct entry *entry)
{
	struct selector *sel;

	for (; entry; entry = entry->next) {
		printf("%s\n", entry->text);
		for (sel = entry->sel; sel; sel = sel->next) {
			printf("@");
			printf("\n\tmins: ");
			print_constraint(&sel->min);
			printf("\n\thours: ");
			print_constraint(&sel->hour);
			printf("\n\tdow: ");
			print_constraint(&sel->dow);
			printf("\n\tdom: ");
			print_constraint(&sel->dom);
			printf("\n\tmonth: ");
			print_constraint(&sel->month);
			printf("\n\tyear: ");
			print_constraint(&sel->year);
			printf("\n\tdur: %ld\n", sel->dur);
		}
	}
}

int
main(int argc, char *argv[])
{
	int opt;
	struct entry *entry;
	time_t begin, end;

	while ((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		default: /* '?' */
			fprintf(stderr, "Usage: %s TODO", argv[0]);
			exit(1);
		}
	}

	if (parse_entries(&entry) < 0) {
		errexit(concat_errctx());
	}
	print_entries(entry);

	return 0;
}
