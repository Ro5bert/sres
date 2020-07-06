
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#define TIMELINE_START 8
#define TIMELINE_STOP 18

#define and ";"
#define clear ";"
#define fgcol(col) "38;5;" #col
#define bgcol(col) "48;5;" #col
#define bold "1"
#define invert "7"
#define ansifmt(style) "\x1b[" style "m"

/* #define BLOCK_FULL "\u2588" */
#define BLOCK_THIRD "\u258d" /* actually 3/8s */

enum day {
	/* 0 is used as array terminator, so start at 1 */
	Mo=1, Tu, We, Th, Fr
};

struct timespan {
	int begin;
	int end;
};

struct section {
	struct section *next;
	char *crn;
	char *sec;
	enum day days[4]; /* at most 3 days + null terminator */
	struct timespan ts;
	int cap, act;
	int wlcap, wlact;
};

struct course {
	struct course *next;
	char *id;
	struct section *sec;
};

struct bin {
	struct bin *next;
	struct section **secs;
};

void
error(char *msg)
{
	fprintf(stderr, msg);
	fprintf(stderr, "\n");
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
gettok(char **s, char delim)
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

void
parse_section_days(enum day (*days)[4], char *s)
{
	int i;

	i = 0;
	stripws(&s);
	while (*s != '\0') {
		if (i == 3) {
			error("parse_section_days: too many days");
		}

		if (strncmp(s, "Mo", 2) == 0) {
			(*days)[i++] = Mo;
		} else if (strncmp(s, "Tu", 2) == 0) {
			(*days)[i++] = Tu;
		} else if (strncmp(s, "We", 2) == 0) {
			(*days)[i++] = We;
		} else if (strncmp(s, "Th", 2) == 0) {
			(*days)[i++] = Th;
		} else if (strncmp(s, "Fr", 2) == 0) {
			(*days)[i++] = Fr;
		} else {
			error("parse_section_days: invalid day");
		}

		if (*++s != '\0') {
			++s;
		}
	}
	(*days)[i] = 0;
}

void
parse_section_timespan(struct timespan *ts, char *s)
{
	char *tok;

	/* TODO: error check strtol */
	tok = gettok(&s, '-');
	ts->begin = strtol(tok, NULL, 10);
	ts->end = strtol(s, NULL, 10);
}

void
parse_section(struct section *sec, char **line, size_t *len)
{
	char *s, *tok;

	for (;;) {
		if (getline(line, len, stdin) == -1) {
			error("parse_section: could not read line");
		}
		s = *line;
		skipws(&s);
		if (*s == '#' || *s == '\0') {
			/* ignore line comment and empty line */
			continue;
		}
		/* got a line containing section info */
		break;
	}

	tok = gettok(&s, ',');
	stripws(&tok);
	if (strlen(tok) != 5) {
		error("parse_section: CRN not 5 chars");
	}
	sec->crn = safemalloc(6);
	strcpy(sec->crn, tok);

	if (s == NULL) {
		error("parse_section: no sec");
	}
	tok = gettok(&s, ',');
	stripws(&tok);
	if (strlen(tok) != 3) {
		error("parse_section: sec not 3 chars");
	}
	sec->sec = safemalloc(4);
	strcpy(sec->sec, tok);

	if (s == NULL) {
		error("parse_section: no days");
	}
	tok = gettok(&s, ',');
	parse_section_days(&sec->days, tok);

	if (s == NULL) {
		error("parse_section: no timespan");
	}
	tok = gettok(&s, ',');
	parse_section_timespan(&sec->ts, tok);

	/* TODO: error check strtol */
	if (s == NULL) {
		error("parse_section: no cap");
	}
	tok = gettok(&s, ',');
	sec->cap = strtol(tok, NULL, 10);

	if (s == NULL) {
		error("parse_section: no act");
	}
	tok = gettok(&s, ',');
	sec->act = strtol(tok, NULL, 10);

	if (s == NULL) {
		error("parse_section: no wlcap");
	}
	tok = gettok(&s, ',');
	sec->wlcap = strtol(tok, NULL, 10);

	if (s == NULL) {
		error("parse_section: no wlact");
	}
	tok = gettok(&s, ',');
	sec->wlact = strtol(tok, NULL, 10);
}

void
parse_courses(struct course **result)
{
	char *line, *s, *tok;
	size_t len;
	long nsecs;
	struct course **crs;
	struct section **sec;

	line = NULL;
	len = 0;
	crs = result;
	while (getline(&line, &len, stdin) != -1) {
		assert(len > 0);
		s = line;
		skipws(&s);
		if (*s == '#' || *s == '\0') {
			/* ignore line comment and empty line */
			continue;
		}

		/* got a line containing course info */
		*crs = safemalloc(sizeof **crs);
		(*crs)->sec = NULL;
		(*crs)->id = safemalloc(8);
		tok = gettok(&s, ',');
		/* XXXXXXX\0X\n
		 * ^        ^
		 * tok      s
		 */
		if (s - tok > 8) {
			error("parse_courses: id too long");
		}
		strcpy((*crs)->id, tok);
		/* TODO: error check strtol */
		nsecs = strtol(s, NULL, 10);
		for (sec = &(*crs)->sec; nsecs > 0; --nsecs, sec = &(*sec)->next) {
			*sec = safemalloc(sizeof **sec);
			(*sec)->next = NULL;
			parse_section(*sec, &line, &len);
		}
		crs = &(*crs)->next;
	}
}

void
pack(struct bin *bin, struct course *head, enum day day)
{
	struct course *crs;
	struct section *sec;
	enum day *d;
	int i;

	bin->secs = safemalloc(64 * sizeof bin->secs[0]);
	i = 0;
	for (crs = head; crs != NULL; crs = crs->next) {
		for (sec = crs->sec; sec != NULL; sec = sec->next) {
			for (d = sec->days; *d != 0; ++d) {
				if (*d == day) {
					if (i == 64) {
						/* TODO */
						error("array not big enough! fix me.");
					}
					bin->secs[i++] = sec;
					break;
				}
			}
		}
	}
}

int
timespan_overlap(struct timespan ts1, struct timespan ts2)
{
	return (ts2.begin <= ts1.begin && ts1.begin <= ts2.end) || 
		(ts2.begin <= ts1.end && ts1.end <= ts2.end);
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

int
main(void)
{
	struct course *crs;
	struct bin bins[6]; /* bins[0] is unused */
	enum day day;

	parse_courses(&crs);
	for (day = Mo; day <= Fr; ++day) {
		pack(&bins[day], crs, day);
	}
	print_timeline();
	printf("\n");
	// print buckets by week day

	return 0;
}
