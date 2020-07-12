
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include "sres.h"

bool
cons_try_merge(struct constraint *a, struct constraint *b)
{
	if ((b->begin - a->end <= 1 && a->begin <= b->end) ||
			(a->begin - b->end <= 1 && b->begin <= a->end)) {
		a->begin = min(a->begin, b->begin);
		a->end = max(a->end, b->end);
		return true;
	}
	return false;
}

void
cons_normalize(struct constraint **arr, int *len)
{
	int i, j;
	struct constraint t;

	/* Wildcard: no simplifications to be made. */
	if ((*arr)[0].begin < 0) return;

	/* Insertion sort. */
	for (i = 1; i < *len; ++i) {
		for (j = i; (*arr)[j].begin < (*arr)[j-1].begin && j > 0; --j) {
			t = (*arr)[j];
			(*arr)[j] = (*arr)[j-1];
			(*arr)[j-1] = t;
		}
	}

	/* Try to merge adjacent constraints. */
	for (i = 0, j = 1; j < *len; ++j) {
		if (!cons_try_merge(&(*arr)[i], &(*arr)[j])) {
			(*arr)[++i] = (*arr)[j];
		}
	}
	*len = i+1;
}

bool
cons_next(int *val, struct constraint *cons, int len, int max)
{
	int i;

	if (cons[0].begin < 0) {
		*val += 1;
		if (*val > max) {
			*val = -1;
			return false;
		}
	} else {
		for (i = 0; i < len && *val > cons[i].end; ++i)
			;
		if (i < len && cons[i].end == *val) {
			++i;
		}
		if (i >= len) {
			*val = -1;
			return false;
		}
		assert(*val < cons[i].end);
		if (inrange(*val, cons[i].begin, cons[i].end)) {
			*val += 1;
		} else {
			*val = cons[i].begin;
		}
	}

	return true;
}

bool
cons_satisfies(int val, struct constraint *cons, int len)
{
	int i;

	if (cons[0].begin < 0) {
		return true;
	}
	/* TODO: cons is sorted, so this can be a bit smarter. */
	for (i = 0; i < len; ++i) {
		if (inrange(val, cons[i].begin, cons[i].end)) {
			return true;
		}
	}

	return false;
}

void
iter_calc_time(struct entry_iter *i)
{
	struct tm tm;

	tm.tm_year = i->year - 1900;
	tm.tm_mon = i->month;
	tm.tm_mday = i->dom + 1;
	tm.tm_hour = i->hour;
	tm.tm_min = i->min;
	tm.tm_sec = 0;
	tm.tm_isdst = -1;
	i->curr = mktime(&tm);
	/* At this point, mktime could have normalized the fields of tm such
	 * that the described time is invalid (does not satisfy the
	 * constraints). Also, we need to make sure the weekday, which mktime
	 * infers from the other fields, satisfies its constraints. */
	if (!cons_satisfies(tm.tm_wday, i->entry->dow, i->entry->dowl) ||
			tm.tm_min != i->min ||
			tm.tm_hour != i->hour ||
			tm.tm_mday != i->dom+1 ||
			tm.tm_mon != i->month ||
			tm.tm_year != i->year-1900) {
		i->curr = -1; /* Result is invalid. */
	} else {
		i->dow = tm.tm_wday;
	}
}

bool
iter_next(struct entry_iter *i)
{
	struct entry *e;

	e = i->entry; /* For brevity. */
	for (i->curr = -1; i->curr < 0; iter_calc_time(i)) {
		if (!cons_next(&i->min, e->min, e->minl, 59)) {
			if (!cons_next(&i->hour, e->hour, e->hourl, 23)) {
				if (!cons_next(&i->dom, e->dom, e->doml, 30)) {
					if (!cons_next(&i->month, e->month, e->monthl, DEC)) {
						if (!cons_next(&i->year, e->year, e->yearl, 2037)) {
							return false;
						}
						i->month = 0;
						assert(cons_next(&i->month, e->month, e->monthl, DEC));
					}
					i->dom = 0;
					assert(cons_next(&i->dom, e->dom, e->doml, 30));
				}
				i->hour = 0;
				assert(cons_next(&i->hour, e->hour, e->hourl, 23));
			}
			i->min = 0;
			assert(cons_next(&i->min, e->min, e->minl, 59));
		}
	}
	if (i->curr > i->end) {
		i->curr = -1;
		return false;
	}

	return true;
}

bool
iter_init_helper(
		bool *allow_any,
		int *field,
		struct constraint *cons,
		int len,
		int max,
		int begin
)
{
	if (*allow_any) {
		*field = 0;
		if (!cons_satisfies(0, cons, len)) {
			assert(cons_next(field, cons, len, max));
		}
	} else {
		*field = begin;
		if (!cons_satisfies(begin, cons, len)) {
			if (!cons_next(field, cons, len, max)) {
				return false;
			}
			*allow_any = true;
		}
	}
	return true;
}

bool
iter_init(struct entry_iter *i, struct tm *begin)
{
	bool allow_any;
	struct entry *e;

	allow_any = false;
	e = i->entry; /* For brevity. */
	i->curr = -1;

	if (!iter_init_helper(
			&allow_any, &i->year, e->year,
			e->yearl, 2037, begin->tm_year + 1900
	)) return false;
	if (!iter_init_helper(
			&allow_any, &i->month, e->month,
			e->monthl, DEC, begin->tm_mon
	)) return false;
	if (!iter_init_helper(
			&allow_any, &i->dom, e->dom,
			e->doml, 30, begin->tm_mday - 1
	)) return false;
	if (!iter_init_helper(
			&allow_any, &i->hour, e->hour,
			e->hourl, 23, begin->tm_hour
	)) return false;
	if (!iter_init_helper(
			&allow_any, &i->min, e->min,
			e->minl, 59, begin->tm_min
	)) return false;

	iter_calc_time(i);
	if (i->curr < 0) {
		return iter_next(i);
	} else if (i->curr > i->end) {
		i->curr = -1;
		return false;
	}

	return true;
}

/* iter_eq returns true if a and b refer to the same event, and false
 * otherwise. */
bool
iter_eq(struct entry_iter *a, struct entry_iter *b)
{
	/* When two entries have the same text in the input, they are given the
	 * same text pointer, so comparing pointers below is sufficient (instead
	 * of, e.g., using strcmp). */
	return a->curr == b->curr &&
		a->entry->text == b->entry->text &&
		a->entry->dur == b->entry->dur;
}

void
iters_sort(struct entry_iter *iters, int len)
{
	int i, j;
	struct entry_iter t;

	/* Insertion sort. */
	for (i = 1; i < len; ++i) {
		for (j = i; iters[j].curr <= iters[j-1].curr && j > 0; --j) {
			if (iter_eq(&iters[j], &iters[j-1])) {
				iters[j].skip = true;
			}
			t = iters[j];
			iters[j] = iters[j-1];
			iters[j-1] = t;
		}
	}
}

void
iters_resort(struct entry_iter *iters, int len)
{
	int i;
	struct entry_iter t;

	for (i = 1; i < len && iters[i-1].curr >= iters[i].curr; ++i) {
		if (iter_eq(&iters[i-1], &iters[i])) {
			iters[i-1].skip = true;
		}
		t = iters[i-1];
		iters[i-1] = iters[i];
		iters[i] = t;
	}
}

/* TODO TEMP */
void
print_iter(struct entry_iter *iter)
{
	printf("skip: %d\n", iter->skip);
	printf("curr: %ld\n", iter->curr);
	printf("min: %d\n", iter->min);
	printf("hour: %d\n", iter->hour);
	printf("dow: %d\n", iter->dow);
	printf("dom: %d\n", iter->dom);
	printf("month: %d\n", iter->month);
	printf("year: %d\n", iter->year);
}

int
main(int argc, char **argv)
{
	int opt, i, n_entries;
	struct tm begin;
	time_t t, end;
	struct entry *entry, *e;
	struct entry_iter *iters;

	while ((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		/* TODO -f option to change output format */
		/* TODO -c option to control colour */
		default: /* '?' */
			fprintf(stderr, "Usage: %s TODO", argv[0]);
			exit(1);
		}
	}
	
	/* TODO TEMP */
	t = time(NULL);
	if (t < 0) {
		errexit("time failed");
	}
	localtime_r(&t, &begin);
	end = t + 60*60*24; /* one day */

	if (parse_entries(&entry) < 0) {
		errexit(concat_errctx());
	}
	print_entries(entry);
	for (n_entries = 0, e = entry; e; e = e->next) {
		if (e->dur < 0) {
			continue;
		}
		cons_normalize(&e->min, &e->minl);
		cons_normalize(&e->hour, &e->hourl);
		cons_normalize(&e->dow, &e->dowl);
		cons_normalize(&e->dom, &e->doml);
		cons_normalize(&e->month, &e->monthl);
		cons_normalize(&e->year, &e->yearl);
		if (++n_entries == INT_MAX) {
			errexit("too many entries");
		}
	}
	iters = malloc_or_exit(n_entries * sizeof *iters);
	for (i = 0, e = entry; e; e = e->next) {
		if (e->dur < 0) {
			continue;
		}
		iters[i].entry = e;
		iters[i].end = end;
		iters[i].skip = false;
		iter_init(&iters[i], &begin);
		++i;
	}
	iters_sort(iters, n_entries);
	/* After sorting, all the invalid iters (curr < 0) are at the beginning of
	 * the array, so skip over them. */
	i = 0;
	while (iters[i].curr < 0) {
		++i;
	}
	while (i < n_entries) {
		if (!iters[i].skip) {
			/* TODO print event */
			/* TODO TEMP */
			printf("%s\n", iters[i].entry->text);
		} else {
			iters[i].skip = false;
		}
		if (!iter_next(&iters[i])) {
			/* We're done with this iterator. */
			++i;
		} else {
			iters_resort(&iters[i], n_entries-i);
		}
	}

	return 0;
}
