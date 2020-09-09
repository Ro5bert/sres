#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "arg.h"
#include "config.h"
#include "sres.h"

char *argv0;

/* TODO DST is broken: times in skipped hour are included in output */
/* TODO iteration idea: work in Unix time, adding to a single llong counter */

void
entry_init(struct entry *e)
{
	spanarr_init(&e->min);
	spanarr_init(&e->hour);
	spanarr_init(&e->dow);
	spanarr_init(&e->dom);
	spanarr_init(&e->mon);
	spanarr_init(&e->year);
	e->text = NULL;
	e->dur = 0;
	e->next = NULL;
}

bool
entryiter_init(struct entryiter *ei, struct dtime *begin)
{
	size_t i;
	int dowv;

	spaniter_zero(spaniter(ei, min));
	spaniter_zero(spaniter(ei, hour));
	spaniter_zero(spaniter(ei, dom));
	spaniter_zero(spaniter(ei, mon));
	spaniter_zero(spaniter(ei, year));

	for (i = 0; i < 7; ++i)
		ei->dow[i] = false;
	for (i = 0; i < ei->e->dow.len; ++i) {
		dowv = ei->e->dow.spans[i].begin;
		for (; dowv <= ei->e->dow.spans[i].end; ++dowv)
			ei->dow[dowv] = true;
	}

	/* If a field (e.g., year) has been set to a value strictly greater than it
	 * needs to be set to, the fields representing smaller division of time can
	 * be set to their smallest value. E.g., if the beginning date is 12 July
	 * 2020, but the smallest permissible year > 2020 is 2021, then the month
	 * needn't be July or greater; rather, the month (and the dom, hour, and
	 * minute) can be their smallest permissible values. This rule is
	 * implemented in the following. */
	if (!spaniter_seek(spaniter(ei, year), begin->year)) return false;
	if (ei->dt.year > begin->year) goto done;
	if (!spaniter_seek(spaniter(ei, mon), begin->mon)) goto next_year;
	if (ei->dt.mon > begin->mon) goto done;
	if (!spaniter_seek(spaniter(ei, dom), begin->dom)) goto next_mon;
	if (ei->dt.dom > begin->dom) goto done;
	if (!spaniter_seek(spaniter(ei, hour), begin->hour)) goto next_dom;
	if (ei->dt.hour > begin->hour) goto done;
	if (spaniter_seek(spaniter(ei, min), begin->min)) goto done;
	if (!spaniter_next(spaniter(ei, hour))) goto done;
next_dom:
	if (!spaniter_next(spaniter(ei, dom))) goto done;
next_mon:
	if (!spaniter_next(spaniter(ei, mon))) goto done;
next_year:
	if (spaniter_next(spaniter(ei, year))) return false;

done:
	assert(dtime_cmp(&ei->dt, begin) >= 0);
	return entryiter_stabilizedmy(ei);
}

bool
entryiter_next(struct entryiter *ei)
{
	if (spaniter_next(spaniter(ei, min)) &&
	    spaniter_next(spaniter(ei, hour))) {
		if (spaniter_next(spaniter(ei, dom)) &&
		    spaniter_next(spaniter(ei, mon)) &&
		    spaniter_next(spaniter(ei, year)))
			return false; /* Year wrapped around. */
		return entryiter_stabilizedmy(ei);
	}
	return true;
}

bool
entryiter_stabilizedmy(struct entryiter *ei)
{
	if (!dtime_calcdow(&ei->dt) || !ei->dow[ei->dt.dow]) {
	    spaniter_zero(spaniter(ei, min));
	    spaniter_zero(spaniter(ei, hour));
		do {
			if (spaniter_next(spaniter(ei, dom)) &&
			    spaniter_next(spaniter(ei, mon)) &&
			    spaniter_next(spaniter(ei, year)))
				return false; /* Year wrapped around. */
		} while (!dtime_calcdow(&ei->dt) || !ei->dow[ei->dt.dow]);
	}
	return true;
}

bool
entryiter_eq(struct entryiter *a, struct entryiter *b)
{
	/* When two entries have the same text in the input, they are given the
	 * same text pointer, so comparing pointers below is sufficient (instead
	 * of, e.g., using strcmp). */
	return dtime_cmp(&a->dt, &b->dt) == 0 &&
		a->e->text == b->e->text &&
		a->e->dur == b->e->dur;
}

void
entryiter_sort(struct entryiter *eis, size_t len)
{
	size_t i, j;
	struct entryiter t;

	/* Insertion sort. */
	for (i = 1; i < len; ++i) {
		for (j = i; j > 0 && dtime_cmp(&eis[j].dt, &eis[j-1].dt) <= 0; --j) {
			if (entryiter_eq(&eis[j], &eis[j-1]))
				eis[j].skip = true;
			t = eis[j];
			eis[j] = eis[j-1];
			eis[j-1] = t;
		}
	}
}

void
entryiter_resort(struct entryiter *eis, size_t len)
{
	size_t i;
	struct entryiter t;

	for (i = 1; i < len && dtime_cmp(&eis[i-1].dt, &eis[i].dt) >= 0; ++i) {
		if (entryiter_eq(&eis[i-1], &eis[i]))
			eis[i-1].skip = true;
		t = eis[i-1];
		eis[i-1] = eis[i];
		eis[i] = t;
	}
}

void
spanarr_init(struct spanarr *arr)
{
	arr->spans = NULL;
	arr->len = 0;
	arr->cap = 0;
}

bool
spanarr_insert(struct spanarr *arr, struct span span)
{
	size_t i, j;

	/* Try to merge into an existing span. */
	for (i = 0; i < arr->len; ++i) {
		if (span_try_merge(&arr->spans[i], &span)) {
			goto done;
		}
	}

	if (arr->len == arr->cap) { /* Need to grow? */
		if (arr->cap > SIZE_MAX / 2)
			return false; /* Overflow. */
		arr->cap = arr->cap > 0 ? 2*arr->cap : 4;
		arr->spans = realloc_or_exit(arr->spans, arr->cap * sizeof *arr->spans);
	}

	i = 0;
	while (i < arr->len && span.begin >= arr->spans[i].begin)
		++i;
	for (j = arr->len; j > i; --j)
		arr->spans[j] = arr->spans[j-1];
	arr->spans[i] = span;
	++arr->len;

done:
	/* After adding the span, it might be possible to merge adjacent spans. */
	i = 0;
	for (j = 1; j < arr->len; ++j) {
		if (!span_try_merge(&arr->spans[i], &arr->spans[j]))
			arr->spans[++i] = arr->spans[j];
	}
	arr->len = i+1;

	return true;
}

void
spaniter_zero(struct spanarr *arr, Spanv *val, size_t *idx)
{
	assert(arr->len > 0);
	*idx = 0;
	*val = arr->spans[0].begin;
}

bool
spaniter_seek(struct spanarr *arr, Spanv *val, size_t *idx, Spanv target)
{
	size_t i;

	for (i = *idx; i < arr->len; ++i) {
		if (arr->spans[i].end >= target) {
			*val = max(arr->spans[i].begin, target);
			*idx = i;
			return true;
		}
	}

	return false;
}

bool
spaniter_next(struct spanarr *arr, Spanv *val, size_t *idx)
{
	assert(*idx < arr->len);
	assert(inrange(*val, arr->spans[*idx].begin, arr->spans[*idx].end));
	if (*val < arr->spans[*idx].end) {
		++*val;
	} else {
		++*idx;
		if (*idx >= arr->len) { /* Iter wrapped around? */
			spaniter_zero(arr, val, idx);
			return true;
		} else {
			*val = arr->spans[*idx].begin;
		}
	}
	return false;
}

bool
span_try_merge(struct span *a, struct span *b)
{
	if ((b->begin - a->end <= 1 && a->begin <= b->end) ||
			(a->begin - b->end <= 1 && b->begin <= a->end)) {
		a->begin = min(a->begin, b->begin);
		a->end = max(a->end, b->end);
		return true;
	}
	return false;
}

static void
usage(void)
{
	fprintf(stderr,
		"Usage: %s [-f FMT]\n"
		"       %s [-f FMT] [BEGIN] END\n"
		"Take a description of events over standard input, and then\n"
		"output when the events occur between BEGIN and END.\n"
		"BEGIN defaults to now; END defaults to one day from now.\n",
		argv0, argv0
	);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	char *fmt;
	char *beginstr, *endstr;
	struct dtime begin, end;
	struct entry *entry, *e;
	size_t nentries;
	size_t i;
	struct entryiter *eis;

	fmt = DFLT_FMT;

	/* TODO -i INPUT_FIE option? */
	ARGBEGIN {
	case 'f':
		fmt = EARGF(usage());
		break;
	case 'h':
		usage();
	default:
		fprintf(stderr, "unknown option '%c'\n", ARGCURR());
		usage();
	} ARGEND

	beginstr = DFLT_BEGIN;
	endstr = DFLT_END;
	if (*argv != NULL) {
		endstr = *argv;
		argv++;
	}
	if (*argv != NULL) {
		beginstr = endstr;
		endstr = *argv;
		argv++;
	}
	if (*argv != NULL) {
		fprintf(stderr, "too many arguments\n");
		usage();
	}

	if (!parse_instant(&begin, beginstr)) {
		erradd("failed to parse begin time");
		errexit(errget());
	}
	if (!parse_instant(&end, endstr)) {
		erradd("failed to parse end time");
		errexit(errget());
	}

	if (!parse_entries(&entry, &nentries))
		errexit(errget());
	eis = malloc_or_exit(nentries * sizeof *eis);
	i = 0;
	for (e = entry; e; e = e->next) {
		eis[i].e = e;
		if (entryiter_init(&eis[i], &begin))
			++i;
		else
			--nentries;
	}
	entryiter_sort(eis, nentries);

	i = 0;
	while (i < nentries) {
		if (dtime_cmp(&eis[i].dt, &end) > 0) {
			/* The iterator is exhausted. */
			++i;
			continue;
		}
		if (!eis[i].skip) {
			if (!entryiter_printf(fmt, &eis[i]))
				errexit(errget());
		} else {
			eis[i].skip = false;
		}
		if (!entryiter_next(&eis[i]))
			/* The iterator is exhausted. */
			++i;
		else
			entryiter_resort(&eis[i], nentries-i);
	}

	return 0;
}
