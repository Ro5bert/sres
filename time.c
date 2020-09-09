#include <limits.h>
#include <stdbool.h>
#include <time.h>

#include "sres.h"

#define DAYS400Y (303LL*365LL + 97LL*366LL)
#define DAYS100Y ( 76LL*365LL + 24LL*366LL)
#define DAYS4Y   (  3LL*365LL +  1LL*366LL)

#define is_leap_year(y) ((y%4 == 0 && y%100 != 0) || y%400 == 0)

static int jan1dow[400];
static bool jan1dow_inited = false;
static int monthdayscommon[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static int monthdaysleap[12] = {
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static int daysbeforemonth[12] = {
	0,
	31,
	31 + 28,
	31 + 28 + 31,
	31 + 28 + 31 + 30,
	31 + 28 + 31 + 30 + 31,
	31 + 28 + 31 + 30 + 31 + 30,
	31 + 28 + 31 + 30 + 31 + 30 + 31,
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
	31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
};

static void
init_jan1dow(void)
{
	int year, day;

	if (jan1dow_inited)
		return;
	jan1dow_inited = true;

	year = 0;
	/* Jan 1, 1 BC is a Saturday in the proleptic Gregorian calendar. */
	day = SAT;
	for (; year < 400; ++year) {
		jan1dow[year] = day % 7;
		day += 1 + is_leap_year(year);
	}
}

bool
dtime_isdmyvalid(struct dtime *dt)
{
	return inrange(dt->mon, 0, 11) && inrange(dt->dom, 0,
		is_leap_year(dt->year) ? monthdaysleap[dt->mon]-1
		                       : monthdayscommon[dt->mon]-1
	);
}

bool
dtime_calcdow(struct dtime *dt)
{
	int doy;

	init_jan1dow();
	if ((doy = dtime2doy(dt)) < 0)
		return false;
	dt->dow = (jan1dow[dt->year%400] + doy) % 7;
	return true;
}

int
dtime2doy(struct dtime *dt)
{
	if (!dtime_isdmyvalid(dt))
		return -1;
	return daysbeforemonth[dt->mon] + dt->dom +
		(dt->mon > FEB && is_leap_year(dt->year));
}

int
dtime_cmp(struct dtime *a, struct dtime *b)
{
	/* Could probably do this branchlessly, but need to be wary of overflow. */
	if (a->year != b->year) return a->year > b->year ? 1 : -1;
	if (a->mon  != b->mon)  return a->mon  > b->mon  ? 1 : -1;
	if (a->dom  != b->dom)  return a->dom  > b->dom  ? 1 : -1;
	if (a->hour != b->hour) return a->hour > b->hour ? 1 : -1;
	if (a->min  != b->min)  return a->min  > b->min  ? 1 : -1;
	return 0;
}

bool
dtime2tm(struct tm *tm, struct dtime *dt)
{
	if (dt->year < INT_MIN + 1900 || dt->year - 1900 > INT_MAX) {
		errset("dtime year overflows tm year");
		return false;
	}
	tm->tm_year  = dt->year - 1900;
	tm->tm_mon   = dt->mon;
	tm->tm_mday  = dt->dom + 1;
	tm->tm_wday  = dt->dow;
	tm->tm_hour  = dt->hour;
	tm->tm_min   = dt->min;
	tm->tm_sec   = 0;
	tm->tm_isdst = -1;
	return true;
}

/* This does not apply a offset for timezone/DST. */
bool
dtime2min(long long *min, struct dtime *dt)
{
	int doy;
	long long year;

	if ((doy = dtime2doy(dt)) < 0) {
		errset("dom/mon/year incompatible");
		return false;
	}
	if (dt->year > LLONG_MAX/(366LL*1440LL) ||
	    dt->year < LLONG_MIN/(366LL*1440LL)) {
		errset("dtime year overflows min");
		return false;
	}

	/* Epoch at 1 Jan 1BC. If we scaled by 60 and offset by 62167219200LL, we
	 * could convert to Unix time (not accounting for UTC offset). */
	*min = 0;
	*min += dt->min + 60LL*dt->hour + 1440LL*doy;
	year = dt->year;
	*min += DAYS400Y * 1440LL * (year/400);
	year %= 400;
	*min += DAYS100Y * 1440LL * (year/100);
	year %= 100;
	*min += DAYS4Y * 1440LL * (year/4);
	year %= 4;
	*min += 365LL * 1440LL * year;
	if (!is_leap_year(dt->year) && dt->year > 0)
		*min += 1440LL;

	return true;
}

/* This does not apply a offset for timezone/DST. */
bool
min2dtime(struct dtime *dt, long long min)
{
	long long days;
	long long minrem;
	long long y400;
	long long y100;
	long long y4;
	long long yrem;
	enum month mon;
	int *monthdays; /* Either monthdayscommon or monthdaysleap. */

	if (min/(365LL*1440LL) > (long long) SPANV_MAX ||
	    min/(365LL*1440LL) < (long long) SPANV_MIN) {
		errset("min overflows dtime year");
		return false;
	}

	days = min / 1440LL;
	minrem = min % 1440LL;
	if (minrem < 0) {
		minrem += 1440LL;
		days--;
	}
	y400 = days / DAYS400Y;
	days %= DAYS400Y;
	if (days < 0) {
		days += DAYS400Y;
		y400--;
	}
	y100 = days / DAYS100Y;
	days %= DAYS100Y;
	y4 = days / DAYS4Y;
	days %= DAYS4Y;
	yrem = days / 365LL;
	days %= 365LL;
	if (yrem == 0 && (y4 != 0 || y100 == 0)) { /* Leap. */
		monthdays = monthdaysleap;
	} else { /* Not leap. */
		monthdays = monthdayscommon;
		days--;
		if (days < 0) {
			days += 365LL;
			yrem--;
		}
	}
	for (mon = JAN; days >= monthdays[mon]; ++mon)
		days -= monthdays[mon];

	dt->year = 400*y400 + 100*y100 + 4*y4 + yrem;
	dt->mon  = mon;
	dt->dom  = days;
	dt->dow  = -1; /* TODO can probably infer from above calculations */
	dt->hour = minrem / 60LL;
	dt->min  = minrem % 60LL;

	return true;
}

bool
dtime_add(struct dtime *dt, long mins)
{
	long long dtmins;

	if (!dtime2min(&dtmins, dt))
		return false;
	if ((mins > 0 && dtmins > LLONG_MAX - (long long) mins) ||
	    (mins < 0 && dtmins < LLONG_MIN - (long long) mins)) {
		errset("mins overflows dtime");
		return false;
	}
	return min2dtime(dt, dtmins+mins);
}
