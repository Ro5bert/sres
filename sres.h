
#include <time.h>

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define abs(a) ((a) >= 0 ? (a) : -(a))
#define inrange(a,l,u) ((l) <= (a) && (a) <= (u))

typedef enum {false, true} bool;

enum dow {
	SUN, MON, TUE, WED, THU, FRI, SAT
};

enum month {
	JAN, FEB, MAR, APR, MAY, JUN,
	JUL, AUG, SEP, OCT, NOV, DEC
};

struct constraint {
	/* If a constraint is a wildcard, then begin < 0 (and end < 0, although
	 * checking only begin is sufficient). If a constraint is a single value,
	 * it is represented as a zero-length range (i.e., begin == end). */
	int begin, end;
};

struct entry {
	char *text;
	struct constraint *min, *hour, *dow, *dom, *month, *year;
	int minl, hourl, dowl, doml, monthl, yearl;
	long dur;
	struct entry *next;
};

struct entry_iter {
	struct entry *entry;
	time_t curr, end;
	bool skip;
	int min, hour, dow, dom, month, year;
};

/* util.c */
void errexit(char const *msg);
void *malloc_or_exit(size_t n);
void *realloc_or_exit(void *ptr, size_t n);
void push_errctx(char const *msg);
void push_errctx_linecnt(long linecnt);
char *concat_errctx(void);

/* parse.c */
bool parse_entries(struct entry **entry);

/* output.c */
extern char *fmt;
bool print_iter(struct entry_iter *iter);
