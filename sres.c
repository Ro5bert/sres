
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

/* iter_calc_time sets i->curr and i->dow to the values described by the min,
 * hour, dom, month, and year fields of the iterator (using locale
 * information). If the fields do not describe a valid time, or if the inferred
 * DoW is invalid, i->curr is set to -1. */
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
	 * infers from the other fields, satisfies its constraints and set dow if
	 * so. */
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

/* iter_next sets the min, hour, dow, dom, month, and year fields of the
 * iterator to the next valid time less than end, and sets i->curr to the Unix
 * time corresponding to this assignment. If no such time exists, i->curr is
 * set to -1. iter_next returns true if and only if a valid time was found.
 *
 * The implementation ensures that iterating over all events in a given time
 * span takes time proportional to the number of events in that time span. The
 * naive implementation would be to iterate minute-by-minute and repeatedly
 * check if the contraints of an entry satisfies each minute, but this would
 * take time proportional to the size of the time span (i.e., inefficient for
 * large time spans, especially those containing a relatively small number of
 * events). */
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
						assert(cons_next(&i->month, e->month, e->monthl, DEC));
					}
					assert(cons_next(&i->dom, e->dom, e->doml, 30));
				}
				assert(cons_next(&i->hour, e->hour, e->hourl, 23));
			}
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
iter_init(struct entry_iter *i, struct tm *begin)
{
	bool allow_any;
	struct entry *e;

	allow_any = false;
	e = i->entry; /* For brevity. */
	i->curr = -1;

	i->year = begin->tm_year + 1900 - 1;
	while (cons_next(&i->year, e->year, e->yearl, 2037)) {
		/* If year has been set to a value strictly greater than it needs to be
		 * set to, the following fields can be set to their smallest value.
		 * E.g., if the beginning date is 12 July 2020, but the smallest
		 * permissible year is 2021, then the month needn't be July or greater;
		 * rather, the month (and the dom, hour, and minute) can be their
		 * smallest permissible values. This is what allow_any indicates. */
		allow_any = i->year > begin->tm_year+1900;
		i->month = (allow_any ? 0 : begin->tm_mon) - 1;
		while (cons_next(&i->month, e->month, e->monthl, DEC)) {
			allow_any = allow_any || i->month > begin->tm_mon;
			i->dom = (allow_any ? 0 : begin->tm_mday-1) - 1;
			while (cons_next(&i->dom, e->dom, e->doml, 30)) {
				allow_any = allow_any || i->dom > begin->tm_mday-1;
				i->hour = (allow_any ? 0 : begin->tm_hour) - 1;
				while (cons_next(&i->hour, e->hour, e->hourl, 23)) {
					allow_any = allow_any || i->hour > begin->tm_hour;
					i->min = (allow_any ? 0 : begin->tm_min) - 1;
					if (cons_next(&i->min, e->min, e->minl, 59)) {
						/* We've got a date in the timespan, but it might not
						 * be valid (weekday might not satsify constraints, for
						 * example). However, at this point, if this date isn't
						 * valid we can just use iter_next to advance to the
						 * next valid date. */
						goto exit_loop;
					}
				}
			}
		}
	}
exit_loop:
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

void
print_usage(char *argv0)
{
	fprintf(stderr,
		"Usage: %s [-f fmt]\n"
		"       %s [-f fmt] [begin] <end>\n"
		"If begin is not provided, it defaults to now.\n"
		"If end is not provided, it defaults to one day from now.\n"
		"Both begin and end have the format [HHmmDDMMMyyyy][+dur].\n"
		"If the first part of the format is absent, it defaults to now.\n"
		"If the second part of the format is absent, it defaults to zero.\n"
		"See the in-source documentation for more info.\n",
		/* TODO make man page */
		argv0, argv0
	);
}

int
main(int argc, char **argv)
{
	int opt, i, n_entries;
	struct tm tm, begin;
	time_t t, end;
	struct entry *entry, *e;
	struct entry_iter *iters;

	while ((opt = getopt(argc, argv, "f:h")) != -1) {
		switch (opt) {
		case 'f':
			fmt = optarg;
			break;
		case 'h':
		default: /* '?' */
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	/* TODO Note we don't check that the user hasn't entered in some strange
	 * time such as Feb 31. In such cases, mktime would normalize the time and
	 * the user might get unexpected results. */
	if (time(&t) < 0) {
		errexit("could not determine current time");
	}
	if (localtime_r(&t, &tm) == NULL) {
		errexit("could not decompose current time");
	}
	begin = tm;
	switch (argc - optind) {
	case 2:
		if (!parse_endpoint(&begin, argv[optind++])) {
			push_errctx("could not parse begin time");
			errexit(concat_errctx());
		}
		if (mktime(&begin) < 0) { /* Normalize. */
			errexit("could not calculate begin time");
		}
		/* FALLTHROUGH */
	case 1:
		if (!parse_endpoint(&tm, argv[optind])) {
			push_errctx("could not parse end time");
			errexit(concat_errctx());
		}
		if ((end = mktime(&tm)) < 0) {
			errexit("could not calculate end time");
		}
		break;
	case 0:
		tm.tm_mday += 1;
		if ((end = mktime(&tm)) < 0) {
			errexit("could not calculate end time");
		}
		break;
	default:
		fprintf(stderr, "too many arguments");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (!parse_entries(&entry)) {
		errexit(concat_errctx());
	}
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
		if (++n_entries == INT_MAX && e->next) {
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
			if (!print_iter(&iters[i])) {
				errexit(concat_errctx());
			}
		} else {
			iters[i].skip = false;
		}
		if (!iter_next(&iters[i])) {
			/* This iterator is exhausted. */
			++i;
		} else {
			iters_resort(&iters[i], n_entries-i);
		}
	}

	return 0;
}
