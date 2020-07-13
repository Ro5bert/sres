
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include "sres.h"

#define FLAG_0 0x01
#define FLAG_1 0x02
#define FLAG_b 0x04
#define FLAG_c 0x08
#define FLAG_s 0x10

/* %%   a '%' character
 * %n   a newline character
 * %t   a tab character
 * %x   text associated with event
 * %d   duration in minutes
 * %b_  _ corresponding to the beginning of the event
 * %e_  _ corresponding to the end of the event
 * Valid substitutions for _ above:
 *   'm'  minutes (00-59)
 *   'h'  hours (01-12)
 *   'H'  hours (00-23)
 *   'p'  a.m. or p.m.
 *   'd'  day of week (Sunday, Monday, Tuesday, etc.)
 *   'D'  day of month (01-31)
 *   'M'  month (January, February, March, etc.)
 *   'y'  year (4-digit decimal year)
 *   'u'  Unix timestamp
 * Valid prefix modifiers for _ above:
 *   '0'  zero-based numeric
 *        0d  "Sunday" -> 0, "Monday" -> 1, etc.
 *        0M  "January" -> 00, "February" -> 01, etc.
 *   '1'  one-based numeric
 *        1d  "Sunday" -> 1, "Monday" -> 2, etc.
 *        1M  "January" -> 01, "February" -> 02, etc.
 *   'b'  change leading zeros to blanks (spaces)
 *   'c'  change case
 *        cp  "a.m." -> "A.M.", "p.m." -> "P.M."
 *        cd  "Sunday" -> "sunday", "Monday" -> "monday", etc.
 *        cM  "January" -> "january", "February" -> "february", etc.
 *   's'  short
 *        sm  drops leading zero
 *        sh  drops leading zero
 *        sH  drops leading zero
 *        sp  "a.m." -> "am", "p.m." -> "pm"
 *        sd  "Sunday" -> "Sun", "Monday" -> "Mon", etc.
 *        sD  drops leading zero
 *        sM  "January" -> "Jan", "February" -> "Feb", etc.
 *        sy  drops century number (1986 -> 86, 2009 -> 09, etc.)
 * Modifiers can be combined (the order is irrelevant) and the effect is
 * probably what you expect.
 * Invalid modifiers are ignored. */
/* TODO option for week number and year day? */
char *fmt = "%bH:%bm %bD %bsM %by %dm: %x";

static char *days[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

static char *months[] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

static void
print_2digits(int val, unsigned int flags)
{
	if (flags & FLAG_s) {
		printf("%d", val);
	} else if (flags & FLAG_b) {
		printf("%2d", val);
	} else {
		printf("%02d", val);
	}
}

bool
print_iter(struct entry_iter *iter)
{
	int i, f;
	unsigned int flags;
	time_t begint, endt, t;
	struct tm begintm, endtm, *tm;
	char buf[16]; /* Only needs to be big enough for "September". */

	begint = iter->curr;
	begintm.tm_year  = endtm.tm_year  = iter->year - 1900;
	begintm.tm_mon   = endtm.tm_mon   = iter->month;
	begintm.tm_mday  = endtm.tm_mday  = iter->dom + 1;
	begintm.tm_wday  = endtm.tm_wday  = iter->dow;
	begintm.tm_hour  = endtm.tm_hour  = iter->hour;
	begintm.tm_min   = endtm.tm_min   = iter->min;
	begintm.tm_sec   = endtm.tm_sec   = 0;
	begintm.tm_isdst = endtm.tm_isdst = -1;

	endtm.tm_min += iter->entry->dur;
	if ((endt = mktime(&endtm)) < 0) { /* Normalize end. */
		/* This could happen for massive durations, but should be rare. */
		push_errctx("could not represent end time as struct tm");
		return false;
	}

	t = begint;
	tm = &begintm;
	for (i = 0; fmt[i] != '\0'; ++i) {
		if (fmt[i] == '%') {
			switch (fmt[++i]) {
			case '%':
				printf("%%");
				break;
			case 't':
				printf("\t");
				break;
			case 'n':
				printf("\n");
				break;
			case 'x':
				printf("%s", iter->entry->text);
				break;
			case 'd':
				printf("%ld", iter->entry->dur);
				break;
			case 'e':
				t = endt;
				tm = &endtm;
				/* FALLTHROUGH */
			case 'b':
				flags = 0;
again:
				f = 1;
				switch (fmt[++i]) {
				/* Order of these cases is important! */
				case 's': f <<= 1;
				case 'c': f <<= 1;
				case 'b': f <<= 1;
				case '1': f <<= 1;
				case '0':
					if (flags & f) {
						push_errctx("bad fmt: duplicate flag");
						return false;
					}
					flags |= f;
					goto again;
				case 'm':
					print_2digits(tm->tm_min, flags);
					break;
				case 'h':
					if (tm->tm_hour%12 == 0) {
						printf("12");
					} else {
						print_2digits(tm->tm_hour % 12, flags);
					}
					break;
				case 'H':
					print_2digits(tm->tm_hour, flags);
					break;
				case 'p':
					buf[0] = tm->tm_hour >= 12 ? 'p' : 'a';
					buf[1] = '.';
					buf[2] = 'm';
					buf[3] = '.';
					buf[4] = '\0';
					if (flags & FLAG_c) {
						buf[0] = toupper(buf[0]);
						buf[2] = 'M';
					}
					if (flags & FLAG_s) {
						buf[1] = buf[2];
						buf[2] = '\0';
					}
					printf("%s", buf);
					break;
				case 'd':
					if (flags & FLAG_0) {
						printf("%d", tm->tm_wday);
					} else if (flags & FLAG_1) {
						printf("%d", tm->tm_wday + 1);
					} else {
						assert(tm->tm_wday < 7);
						strcpy(buf, days[tm->tm_wday]);
						if (flags & FLAG_c) {
							buf[0] = tolower(buf[0]);
						}
						if (flags & FLAG_s) {
							buf[3] = '\0';
						}
						printf("%s", buf);
					}
					break;
				case 'D':
					print_2digits(tm->tm_mday, flags);
					break;
				case 'M':
					if (flags & FLAG_0) {
						print_2digits(tm->tm_mon, flags);
					} else if (flags & FLAG_1) {
						print_2digits(tm->tm_mon + 1, flags);
					} else {
						assert(tm->tm_mon < 12);
						strcpy(buf, months[tm->tm_mon]);
						if (flags & FLAG_c) {
							buf[0] = tolower(buf[0]);
						}
						if (flags & FLAG_s) {
							buf[3] = '\0';
						}
						printf("%s", buf);
					}
					break;
				case 'y':
					if (flags & FLAG_s) {
						printf("%d", (tm->tm_year + 1900) % 100);
					} else {
						printf("%d", tm->tm_year + 1900);
					}
					break;
				case 'u':
					printf("%ld", t);
					break;
				default: /* Includes fmt[i] == '\0'. */
					push_errctx("bad fmt: invalid conversion specifier");
					return false;
				}
				t = begint;
				tm = &begintm;
				break;
			default: /* Includes fmt[i] == '\0'. */
				push_errctx("bad fmt: invalid conversion specifier");
				return false;
			}
		} else {
			printf("%c", fmt[i]);
		}
	}
	printf("\n");
	
	return true;
}
