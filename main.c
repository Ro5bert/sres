
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>

/* Half the number of characters spanned by a single hour.
 * Must be at least 1. */
#define DEF_HRESOLUTION 4

#define and ";"
#define clear ";"
#define fgcol(col) "38;5;" #col
#define bgcol(col) "48;5;" #col
#define bold "1"
#define invert "7"
#define ansifmt(style) "\x1b[" style "m"

#define ARROW "\u2191"
#define ELLIPSIS "\u2026"

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
	struct sel_constraint t, dow, dom, m, y;
	long dur;
	struct selector *next;
};

struct entry {
	char *title;
	char *desc;
	struct selector *sel;
	struct entry *next;
};

void
error(char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	exit(1);
}

void *
safemalloc(size_t n)
{
	void *ptr;

	if ((ptr = malloc(n)) == NULL) {
		error("out of memory");
	}
	return ptr;
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
time2num(char *s)
{
	int h, m;

	if (strlen(s) != 4) {
		error("bad time: invalid length");
	}
	if (!isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[2]) || !isdigit(s[3])) {
		error("bad time: contains non digits");
	}
	h = (s[0] - '0') * 10 + (s[1] - '0');
	if (h < 0 || h > 23) {
		error("bad time: hour out of bounds");
	}
	m = (s[2] - '0') * 10 + (s[3] - '0');
	if (m < 0 || m > 59) {
		error("bad time: minute out of bounds");
	}

	return h * 60 + m;
}

int
dow2num(char *s)
{
	int dow;

	if (strlen(s) != 2) {
		error("bad day of week: invalid length");
	}
	s[0] = tolower(s[0]);
	s[1] = tolower(s[1]);
	if      (strcmp(s, "su") == 0) dow = SU;
	else if (strcmp(s, "mo") == 0) dow = MO;
	else if (strcmp(s, "tu") == 0) dow = TU;
	else if (strcmp(s, "we") == 0) dow = WE;
	else if (strcmp(s, "th") == 0) dow = TH;
	else if (strcmp(s, "fr") == 0) dow = FR;
	else if (strcmp(s, "sa") == 0) dow = SA;
	else error("bad day of week");

	return dow;
}

int
dom2num(char *s)
{
	int dom;

	dom = 0;
	switch (strlen(s)) {
	case 2:
		if (!isdigit(*s)) error("bad day of month: contains non digits");
		dom = (*s - '0') * 10;
		++s;
	case 1:
		if (!isdigit(*s)) error("bad day of month: contains non digits");
		dom += s[0] - '0';
		break;
	default:
		error("bad day of month: invalid length");
	}
	if (dom < 1 || dom > 31) {
		error("bad day of month: out of bounds");
	}

	return dom;
}

int
month2num(char *s)
{
	int m;

	if (strlen(s) != 3) {
		error("bad month: invalid length");
	}
	s[0] = tolower(s[0]);
	s[1] = tolower(s[1]);
	s[2] = tolower(s[2]);
	if      (strcmp(s, "jan") == 0) m = JAN;
	else if (strcmp(s, "feb") == 0) m = FEB;
	else if (strcmp(s, "mar") == 0) m = MAR;
	else if (strcmp(s, "apr") == 0) m = APR;
	else if (strcmp(s, "may") == 0) m = MAY;
	else if (strcmp(s, "jun") == 0) m = JUN;
	else if (strcmp(s, "jul") == 0) m = JUL;
	else if (strcmp(s, "aug") == 0) m = AUG;
	else if (strcmp(s, "sep") == 0) m = SEP;
	else if (strcmp(s, "oct") == 0) m = OCT;
	else if (strcmp(s, "nov") == 0) m = NOV;
	else if (strcmp(s, "dec") == 0) m = DEC;
	else error("bad month");

	return m;
}

int
year2num(char *s)
{
	char *end;
	long l;

	l = strtol(s, &end, 10);
	if (end == s) {
		error("bad year: no digits");
	}
	if (l < 0 || l > INT_MAX) {
		error("bad year: out of bounds");
	}

	return (int) l;
}

void
parse_sel_constraint(struct sel_constraint *head, char *s, int (*str2num)(char*))
{
	char *tok1, *tok2;
	struct sel_constraint **sc;

	head->next = NULL;
	sc = &head;
	if (strchr(s, '*')) {
		if (strlen(s) > 1) {
			error("invalid use of wildcard");
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
				(*sc)->span.begin = str2num(tok1);
				(*sc)->span.end = str2num(tok2);
				if ((*sc)->span.end <= (*sc)->span.begin) {
					error("invalid range");
				}
			} else {
				(*sc)->type = SEL_AT;
				(*sc)->at = str2num(tok1);
			}
			sc = &(*sc)->next;
		}
	}
}

void
parse_sel_duration(struct selector *sel, char *s)
{
	char *c;
	long l, dur, scale;

	dur = 0;
	scale = 1;
	for (c = s; *c; ++c) {
		if (!isdigit(*c)) {
			switch (*c) {
			case 'w':
				scale *= 7;
			case 'd':
				scale *= 24;
			case 'h':
				scale *= 60;
			case 'm':
				if (c == s) {
					error("bad duration: empty component");
				}
				l = strtol(s, NULL, 10);
				if (l < 0) {
					error("bad duration: negative");
				}
				if (l > LONG_MAX / scale) {
					error("bad duration: too big");
				}
				l *= scale;
				if (dur > LONG_MAX - l) {
					error("bad duration: too big");
				}
				dur += l;
				scale = 1;
				s = c + 1;
				break;
			default:
				error("bad duration: invalid character");
			}
		}
	}
	if (*s != '\0') {
		error("bad duration: missing unit");
	}
	sel->dur = dur;
}

void
parse_selector(struct selector *sel, char *s)
{
	char *tok;

	++s; /* skip over '@' */

	skipws(&s);
	tok = nexttok(&s, ' ');
	parse_sel_constraint(&sel->t, tok, time2num);

	skipws(&s);
	tok = nexttok(&s, ' ');
	parse_sel_constraint(&sel->dow, tok, dow2num);

	skipws(&s);
	tok = nexttok(&s, ' ');
	parse_sel_constraint(&sel->dom, tok, dom2num);

	skipws(&s);
	tok = nexttok(&s, ' ');
	parse_sel_constraint(&sel->m, tok, month2num);

	skipws(&s);
	tok = nexttok(&s, ' ');
	parse_sel_constraint(&sel->y, tok, year2num);

	skipws(&s);
	tok = nexttok(&s, ' ');
	parse_sel_duration(sel, tok);
}

void
parse_entries(struct entry **entry)
{
	char *line, *s, *tok;
	size_t len;
	struct selector **sel;

	*entry = NULL;
	line = NULL;
	len = 0;
	sel = NULL;
	while (getline(&line, &len, stdin) != -1) {
		assert(len > 0);
		s = line;
		stripws(&s);
		if (*s == '#' || *s == '\0') {
			/* ignore line comment and empty line */
			continue;
		}
		if (*s != '@') { /* new entry */
			*entry = safemalloc(sizeof **entry);
			(*entry)->desc = NULL;
			(*entry)->sel = NULL;
			(*entry)->next = NULL;
			sel = &(*entry)->sel;
			tok = nexttok(&s, ':');
			(*entry)->title = safemalloc(strlen(tok)+1);
			strcpy((*entry)->title, tok);
			tok = nexttok(&s, ':');
			if (tok) {
				(*entry)->desc = safemalloc(strlen(tok)+1);
				strcpy((*entry)->desc, tok);
			}
			entry = &(*entry)->next;
		} else { /* selector for prev entry */
			if (sel == NULL) {
				error("selector without title");
			}
			*sel = safemalloc(sizeof **sel);
			(*sel)->next = NULL;
			parse_selector(*sel, s);
			sel = &(*sel)->next;
		}
	}
}

void
pack()
{
}

void
print_timeline(void)
{
	int i;
	printf("    ");
	for (i = TIMELINE_START; i <= TIMELINE_STOP; ++i) {
		printf("%02d      ", i);
	}
	printf("\n");
}

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
print_entry(struct entry *entry)
{
	struct selector *sel;

	for (; entry; entry = entry->next) {
		printf("title: %s\n", entry->title);
		printf("desc: %s\n", entry->desc);
		for (sel = entry->sel; sel; sel = sel->next) {
			printf("@");
			printf("\n\ttime: ");
			print_constraint(&sel->t);
			printf("\n\tdow: ");
			print_constraint(&sel->dow);
			printf("\n\tdom: ");
			print_constraint(&sel->dom);
			printf("\n\tmonth: ");
			print_constraint(&sel->m);
			printf("\n\tyear: ");
			print_constraint(&sel->y);
			printf("\n\tdur: %ld\n", sel->dur);
		}
	}
}

int
main(void)
{
	struct entry *entry;
	time_t t = time(NULL);

	parse_entries(&entry);
	/* for (day = Mo; day <= Fr; ++day) {
		pack(&bins[day], crs, day);
	} */
	/* print_timeline(); */
	print_entry(entry);

	return 0;
}
