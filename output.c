#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sres.h"

#define FLAG_0 0x01
#define FLAG_1 0x02
#define FLAG_b 0x04
#define FLAG_c 0x08
#define FLAG_s 0x10

typedef bool handler(struct dtime *dt, time_t u,
                     bool uvalid, unsigned int flags);

static handler print_m;
static handler print_h;
static handler print_H;
static handler print_p;
static handler print_d;
static handler print_D;
static handler print_M;
static handler print_y;
static handler print_Y;
static handler print_e;
static handler print_u;

static unsigned int flagmap[] = {
	['0'] = FLAG_0,
	['1'] = FLAG_1,
	['b'] = FLAG_b,
	['c'] = FLAG_c,
	['s'] = FLAG_s,
};

static handler *handlers[] = {
	['m'] = print_m,
	['h'] = print_h,
	['H'] = print_H,
	['p'] = print_p,
	['d'] = print_d,
	['D'] = print_D,
	['M'] = print_M,
	['y'] = print_y,
	['Y'] = print_Y,
	['e'] = print_e,
	['u'] = print_u,
};

static char *daysofweek[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
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
	"December",
};

static void
print2(Spanv val, unsigned int flags)
{
	if (flags & FLAG_s)
		printf("%d", val);
	else if (flags & FLAG_b)
		printf("%2d", val);
	else
		printf("%02d", val);
}

static bool
print_m(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	print2(dt->min, flags);
	return true;
};

static bool
print_h(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	if (dt->hour == 0 || dt->hour == 12)
		printf("12");
	else
		print2(dt->hour % 12, flags);
	return true;
};

static bool
print_H(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	print2(dt->hour, flags);
	return true;
};

static bool
print_p(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	char buf[5] = "a.m.";
	if (dt->hour >= 12)
		buf[0] = 'p';
	if (flags & FLAG_c) {
		buf[0] = toupper(buf[0]);
		buf[2] = 'M';
	}
	if (flags & FLAG_s) {
		buf[1] = buf[2];
		buf[2] = '\0';
	}
	printf("%s", buf);
	return true;
};

static bool
print_d(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	char buf[16];
	if (flags & FLAG_0) {
		printf("%d", dt->dow);
	} else if (flags & FLAG_1) {
		printf("%d", dt->dow + 1);
	} else {
		assert(0 <= dt->dow && dt->dow < 7);
		strcpy(buf, daysofweek[dt->dow]);
		if (flags & FLAG_c)
			buf[0] = tolower(buf[0]);
		if (flags & FLAG_s)
			buf[3] = '\0';
		printf("%s", buf);
	}
	return true;
};

static bool
print_D(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	print2(dt->dom + ((flags&FLAG_0)==0), flags);
	return true;
};

static bool
print_M(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	char buf[16];
	if (flags & FLAG_0) {
		print2(dt->mon, flags);
	} else if (flags & FLAG_1) {
		print2(dt->mon + 1, flags);
	} else {
		assert(0 <= dt->mon && dt->mon < 12);
		strcpy(buf, months[dt->mon]);
		if (flags & FLAG_c)
			buf[0] = tolower(buf[0]);
		if (flags & FLAG_s)
			buf[3] = '\0';
		printf("%s", buf);
	}
	return true;
};

static bool
print_y(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	if (flags & FLAG_s && dt->year >= 0)
		printf("%02d", dt->year % 100);
	else
		printf("%d", dt->year);
	return true;
};

static bool
print_Y(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	Spanv year;
	year = dt->year;
	if (year < 0) {
		if (year <= SPANV_MIN+1) {
			errset("year overflow");
			return false;
		}
		year = -year + 1;
	}
	if (flags & FLAG_s)
		printf("%02d", year % 100);
	else
		printf("%d", year);
	return true;
};

static bool
print_e(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	char buf[5] = "a.d.";
	if (dt->year <= 0) {
		buf[0] = 'b';
		buf[2] = 'c';
	}
	if (flags & FLAG_c) {
		buf[0] = toupper(buf[0]);
		buf[2] = toupper(buf[2]);
	}
	if (flags & FLAG_s) {
		buf[1] = buf[2];
		buf[2] = '\0';
	}
	printf("%s", buf);
	return true;
};

static bool
print_u(struct dtime *dt, time_t u, bool uvalid, unsigned int flags)
{
	if (!uvalid) {
		errset("time could not be represented as Unix timestamp");
		return false;
	}
	printf("%jd", (intmax_t) u);
	return true;
};

bool
entryiter_printf(char *fmt, struct entryiter *ei)
{
	int i;
	struct dtime begindt, enddt, *dt;
	struct tm tm;
	time_t beginu, endu, u;
	bool uvalid;
	unsigned int flags;
	int f;

	begindt = enddt = ei->dt;
	assert(inrange(begindt.dow, 0, 7));
	if (!dtime_add(&enddt, ei->e->dur))
		return false;
	/* dtime_add does not set dow. */
	assert(dtime_calcdow(&enddt));

	/* We rely on time.h for Unix time conversions (this should probably be
	 * changed), but the times representable by time.h types are not
	 * guaranteed to be as big as we allow with dtime. We calculate the Unix
	 * time representations of the begin and end times in advance, but don't
	 * want to fail with an error when the dtime cannot be represented as a
	 * time_t unless the fmt actually has a %bu/%eu field in it. Hence, we
	 * set uvalid and fail lazily. */
	uvalid = dtime2tm(&tm, &begindt);
	if (uvalid) {
		beginu = mktime(&tm);
		/* mktime modifies tm_isdst iff it succeeds. */
		uvalid = tm.tm_isdst != -1;
		if (uvalid) {
			uvalid = dtime2tm(&tm, &enddt);
			if (uvalid) {
				endu = mktime(&tm);
				uvalid = tm.tm_isdst != -1;
			}
		}
	}

	for (i = 0; fmt[i] != '\0'; ++i) {
		if (fmt[i] != '%') {
			printf("%c", fmt[i]);
			continue;
		}

		switch (fmt[++i]) {
		case '%': printf("%%"); continue;
		case 't': printf("\t"); continue;
		case 'n': printf("\n"); continue;
		case 'x': printf("%s", ei->e->text); continue;
		case 'd': printf("%ld", ei->e->dur); continue;
		case 'e':
			u = endu;
			dt = &enddt;
			break;
		case 'b':
			u = beginu;
			dt = &begindt;
			break;
		default: /* Includes fmt[i] == '\0'. */
			errset("bad fmt: invalid conversion specifier");
			return false;
		}

		flags = 0;

		++i;
		while (inrange(fmt[i], 0, arrlen(flagmap)) &&
		       (f = flagmap[(int)fmt[i]]) != 0) {
			if (flags&f) {
				errset("bad fmt: duplicate flag");
				return false;
			}
			flags |= f;
			++i;
		}

		if (!inrange(fmt[i], 0, arrlen(handlers)) ||
		    handlers[(int)fmt[i]] == NULL) {
			errset("bad fmt: invalid conversion specifier");
			return false;
		}
		if (!handlers[(int)fmt[i]](dt, u, uvalid, flags))
			return false;
	}

	printf("\n");
	
	return true;
}
