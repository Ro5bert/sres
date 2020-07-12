
#include <stdio.h>
#include "sres.h"

static void
print_constraint(struct constraint *cons, int len)
{
	int i;

	for (i = 0; i < len; ++i) {
		if (cons[i].begin < 0) {
			printf("*");
		} else {
			printf("%d-%d", cons[i].begin, cons[i].end);
		}
		if (i < len-1) {
			printf(",");
		}
	}
}

void
print_entries(struct entry *entry)
{
	char *last;

	for (last = NULL; entry; entry = entry->next) {
		if (entry->text != last) printf("%s\n", entry->text);
		last = entry->text;
		printf("@");
		printf("\n\tmins: ");
		print_constraint(entry->min, entry->minl);
		printf("\n\thours: ");
		print_constraint(entry->hour, entry->hourl);
		printf("\n\tdow: ");
		print_constraint(entry->dow, entry->dowl);
		printf("\n\tdom: ");
		print_constraint(entry->dom, entry->doml);
		printf("\n\tmonth: ");
		print_constraint(entry->month, entry->monthl);
		printf("\n\tyear: ");
		print_constraint(entry->year, entry->yearl);
		printf("\n\tdur: %ld\n", entry->dur);
	}
}
