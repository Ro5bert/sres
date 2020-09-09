#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "sres.h"

/* TODO allow tabs as field separator (currently, only spaces work) */

char *
nexttok(char **s, char delim)
{
	char *tok;

	tok = *s;
	if (*s && (*s = strchr(*s, delim))) {
		if (delim != '\0') {
			**s = '\0';
			++*s;
		} else {
			*s = NULL;
		}
	}

	return tok;
}

void
skipws(char **s)
{
	while (isspace(**s))
		++*s;
}

void
stripws(char **s)
{
	char *end;

	skipws(s);
	end = strchr(*s, '\0');
	if (end != *s) { /* In case **s == '\0' after skipws(s). */
		while (isspace(*--end))
			*end = '\0';
	}
}

bool
parse_num(Spanv *n, char **s)
{
	if (!isdigit(**s)) {
		errset("invalid number: starts with non-digit");
		return false;
	}
	*n = 0;
	for (; isdigit(**s); ++*s) {
		if (*n > (SPANV_MAX - (**s-'0')) / 10) {
			errset("invalid number: out of bounds");
			return false;
		}
		*n = 10*(*n) + (**s-'0');
	}
	return true;
}

bool
parse_min(Spanv *min, char **s)
{
	if (!parse_num(min, s)) {
		erradd("invalid minute");
		return false;
	}
	if (*min < 0 || *min >= 60) {
		errset("invalid minute: out of bounds (need 0 <= min < 60)");
		return false;
	}
	return true;
}

bool
parse_hour(Spanv *hour, char **s)
{
	if (!parse_num(hour, s)) {
		erradd("invalid hour");
		return false;
	}
	if (*hour < 0 || *hour >= 24) {
		errset("invalid hour: out of bounds (need 0 <= hour < 24)");
		return false;
	}
	return true;
}

bool
parse_dow(Spanv *dow, char **s)
{
	if      (!strncasecmp(*s, "sun", 3)) *dow = SUN;
	else if (!strncasecmp(*s, "mon", 3)) *dow = MON;
	else if (!strncasecmp(*s, "tue", 3)) *dow = TUE;
	else if (!strncasecmp(*s, "wed", 3)) *dow = WED;
	else if (!strncasecmp(*s, "thu", 3)) *dow = THU;
	else if (!strncasecmp(*s, "fri", 3)) *dow = FRI;
	else if (!strncasecmp(*s, "sat", 3)) *dow = SAT;
	else {
		errset("invalid day of week");
		return false;
	}
	*s += 3;
	return true;
}

bool
parse_dom(Spanv *dom, char **s)
{
	if (!parse_num(dom, s)) {
		erradd("invalid day of month");
		return false;
	}
	if (*dom < 1 || *dom > 31) {
		errset("invalid day of month: out of bounds (need 1 <= dom <= 31)");
		return false;
	}
	*dom -= 1; /* Make dom 0-based like every other contraint. */
	return true;
}

bool
parse_mon(Spanv *mon, char **s)
{
	if      (!strncasecmp(*s, "jan", 3)) *mon = JAN;
	else if (!strncasecmp(*s, "feb", 3)) *mon = FEB;
	else if (!strncasecmp(*s, "mar", 3)) *mon = MAR;
	else if (!strncasecmp(*s, "apr", 3)) *mon = APR;
	else if (!strncasecmp(*s, "may", 3)) *mon = MAY;
	else if (!strncasecmp(*s, "jun", 3)) *mon = JUN;
	else if (!strncasecmp(*s, "jul", 3)) *mon = JUL;
	else if (!strncasecmp(*s, "aug", 3)) *mon = AUG;
	else if (!strncasecmp(*s, "sep", 3)) *mon = SEP;
	else if (!strncasecmp(*s, "oct", 3)) *mon = OCT;
	else if (!strncasecmp(*s, "nov", 3)) *mon = NOV;
	else if (!strncasecmp(*s, "dec", 3)) *mon = DEC;
	else {
		errset("invalid month");
		return false;
	}
	*s += 3;
	return true;
}

bool
parse_year(Spanv *year, char **s)
{
	if (!parse_num(year, s)) {
		erradd("invalid year");
		return false;
	}
	/* Year 0BC and year 0AD don't exist. It goes from 1BC to 1AD. */
	if (*year == 0) {
		errset("invalid year: zero");
		return false;
	}
	if (!strncasecmp(*s, "bc", 2)) {
		*s += 2;
		*year = -*year + 1;
	} else if (!strncasecmp(*s, "ad", 2)) {
		*s += 2;
	}
	return true;
}

bool
parse_span(struct span *span, char *s, bool (*str2num)(Spanv*, char**))
{
	char *begin;

	begin = nexttok(&s, '-');
	if (!str2num(&span->begin, &begin))
		return false;
	if (s) { /* The constraint is a range. */
		if (!str2num(&span->end, &s))
			return false;
		if (span->end <= span->begin) {
			errset("invalid range: need begin < end");
			return false;
		}
	} else { /* The constraint is a single value. */
		span->end = span->begin;
	}

	return true;
}

bool
parse_spanarr(struct spanarr *arr, char *s,
              bool (*str2num)(Spanv*, char**),
              Spanv min, Spanv max)
{
	struct span span;

	if (strchr(s, '*')) {
		if (strlen(s) > 1) {
			errset("invalid use of wildcard");
			return false;
		}
		span.begin = min;
		span.end = max;
		if (!spanarr_insert(arr, span))
			return false;
	} else {
		while (s) {
			if (!parse_span(&span, nexttok(&s, ','), str2num))
				return false;
			if (!spanarr_insert(arr, span))
				return false;
		}
	}

	return true;
}

bool
parse_duration(long *dur, char **s)
{
	int sign;
	int scale;
	long l;

	if (**s == '\0') {
		errset("invalid duration: empty");
		return false;
	}
	*dur = 0;
	while (**s != '\0') {
		sign = 1;
		if (**s == '+' || **s == '-') {
			sign = **s == '+' ? 1 : -1; 
			++*s;
		}
		l = 0;
		if (!isdigit(**s)) {
			errset("invalid duration: invalid character (expected digit)");
			return false;
		}
		for (; isdigit(**s); ++*s) {
			if (l > (LONG_MAX - (**s-'0')) / 10) {
				errset("invalid duration: out of bounds");
				return false;
			}
			l = l*10 + (**s-'0');
		}
		scale = 1;
		switch (**s) {
		/* Order of these cases is important! */
		case 'w': scale *= 7;
		case 'd': scale *= 24;
		case 'h': scale *= 60;
		case 'm':
			if (l > (LONG_MAX) / scale) {
				errset("invalid duration: out of bounds");
				return false;
			}
			l *= sign * scale;
			if ((l > 0 && *dur > LONG_MAX - l) ||
			    (l < 0 && *dur < LONG_MIN - l)) {
				errset("invalid duration: out of bounds");
				return false;
			}
			*dur += l;
			break;
		default:
			errset("invalid duration: invalid character (expected unit)");
			return false;
		}
		++*s;
	}

	return true;
}

/* Helper for parse_entries. */
static bool
parse_field(char **s, struct spanarr *arr, 
            bool (*str2num)(Spanv*, char**),
            Spanv min, Spanv max)
{
	if (*s == NULL) {
		errset("unexpected EOL");
		return false;
	}
	skipws(s);
	return parse_spanarr(arr, nexttok(s, ' '), str2num, min, max);
}

bool
parse_entries(struct entry **entry, size_t *n)
{
	char *line;
	char *s;
	char buf[64];
	size_t len;
	long linecnt;
	struct entry *prev;

	line = NULL;
	len = 0;
	linecnt = 0;
	*entry = prev = NULL;
	while (getline(&line, &len, stdin) != -1) {
		/* Unlikely this could ever happen, but be safe. */
		if (++linecnt == LONG_MAX) {
			errset("too many lines");
			goto err;
		}
		s = line;
		stripws(&s);
		/* Ignore line comments and whitespace-only/empty lines. */
		if (*s == '#' || *s == '\0')
			continue;
		*entry = malloc_or_exit(sizeof **entry);
		entry_init(*entry);
		if (++*n == SIZE_MAX) {
			errset("too many entries");
			goto err;
		}

		/* Start time constraints */
		if (!parse_field(&s, &(*entry)->min,  parse_min,  0,        59))
			goto err;
		if (!parse_field(&s, &(*entry)->hour, parse_hour, 0,        23))
			goto err;
		if (!parse_field(&s, &(*entry)->dow,  parse_dow,  0,        6))
			goto err;
		if (!parse_field(&s, &(*entry)->dom,  parse_dom,  0,        30))
			goto err;
		if (!parse_field(&s, &(*entry)->mon,  parse_mon,  0,        11))
			goto err;
		if (!parse_field(&s, &(*entry)->year, parse_year, YEAR_MIN, YEAR_MAX))
			goto err;

		/* Duration */
		if (s == NULL) {
			errset("unexpected EOL");
			goto err;
		}
		skipws(&s);
		if (!parse_duration(&(*entry)->dur, &(char*){nexttok(&s, ' ')}))
			goto err;
		if ((*entry)->dur < 0) {
			errset("invalid duration: must be nonnegative");
			goto err;
		}

		/* Description */
		if (s != NULL)
			skipws(&s);
		if (s == NULL || *s == '\0') {
			if (prev == NULL) {
				errset("first entry must have description");
				goto err;
			}
			(*entry)->text = prev->text;
		} else {
			(*entry)->text = malloc_or_exit(strlen(s)+1);
			strcpy((*entry)->text, s);
		}

		prev = *entry;
		entry = &(*entry)->next;
	}

	free(line);
	return true;

err:
	snprintf(buf, arrlen(buf), "line %ld", linecnt);
	erradd(buf);
	free(line);
	return false;
}

bool
parse_instant(struct dtime *dt, char *s)
{
	int sign;
	long dur;
	struct tm now;

	/* Format:
	 *   [TIME]/[DATE][(+|-)OFFSET]
	 * More precisely:
	 *   [HOUR[:MIN][am|pm]]/[DOM][MON[YEAR]][(+|-)OFFSET]
	 * Rules:
	 *   TIME blank    => now
	 *   DATE blank    => today
	 *   HOUR provided => MIN defaults to 00
	 *   DOM provided  => MON and YEAR default to current
	 *   MON provided  => DOM defaults to 0 and YEAR defaults to current
	 *   OFFSET defaults to 0m. */

	if (localtime_r(&(time_t){time(NULL)}, &now) == NULL) {
		errset("failed to obtain current time");
		goto err;
	}

	if (isdigit(*s)) {
		if (!parse_hour(&dt->hour, &s))
			goto err;
		if (*s == ':') {
			++s;
			if (!parse_min(&dt->min, &s))
				goto err;
		} else {
			dt->min = 0;
		}
		if (!strncasecmp(s, "am", 2) || !strncasecmp(s, "pm", 2)) {
			if (dt->hour == 0 || dt->hour > 12) {
				errset("invalid hour: out of bounds "
				       "(need 0 < hour <= 12 with am/pm qualifier)");
				goto err;
			}
			dt->hour %= 12;
			if (*s == 'p')
				dt->hour += 12;
			s += 2;
		}
	} else {
		dt->hour = now.tm_hour;
		dt->min = now.tm_min;
	}

	if (*s != '/') {
		errset("expected slash time/date separator");
		goto err;
	}
	++s;

	if (now.tm_year > YEAR_MAX - 1900) {
		/* As if this will ever happen... */
		errset("current year too big");
		goto err;
	}
	dt->year = now.tm_year + 1900;
	dt->dom = -1;
	if (isdigit(*s) && !parse_dom(&dt->dom, &s))
		goto err;
	if (isalpha(*s)) {
		if (!parse_mon(&dt->mon, &s))
			goto err;
		if (dt->dom == -1)
			dt->dom = 0;
		if (isdigit(*s) && !parse_year(&dt->year, &s))
			goto err;
		/* Else, year is already set to current. */
	} else if (dt->dom == -1) {
		dt->dom = now.tm_mday - 1;
		dt->mon = now.tm_mon;
		/* Year is already set to current. */
	}
	if (!dtime_isdmyvalid(dt)) {
		errset("dom/mon/year incompatible");
		goto err;
	}

	if (*s == '+' || *s == '-') {
		sign = *s == '+' ? 1 : -1;
		++s;
		if (!parse_duration(&dur, &s))
			goto err;
		if (sign == -1 && dur == LONG_MIN) {
			errset("invalid duration: out of bounds");
			goto err;
		}
		dur *= sign;
		if (!dtime_add(dt, dur))
			goto err;
	}

	if (*s != '\0') {
		errset("invalid character");
		goto err;
	}

	return true;

err:
	erradd("invalid instant");
	return false;
}
