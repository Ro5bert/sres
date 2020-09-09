/* Requires: limits.h, stdbool.h, time.h */

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define abs(a) ((a) >= 0 ? (a) : -(a))
#define inrange(a,l,u) ((l) <= (a) && (a) <= (u))
#define arrlen(a) (sizeof (a) / sizeof (a)[0])

/* Helper for filling the first 3 args of spaniter_XXX functions. */
#define spaniter(ei, t) &ei->e->t, &ei->dt.t, &ei->t##i

#define SPANV_MAX INT_MAX
#define SPANV_MIN INT_MIN
#define YEAR_MAX SPANV_MAX
#define YEAR_MIN SPANV_MIN

enum dow {
	SUN, MON, TUE, WED, THU, FRI, SAT
};

enum month {
	JAN, FEB, MAR, APR, MAY, JUN,
	JUL, AUG, SEP, OCT, NOV, DEC
};

/* Spanv is the type used to represent minutes, hours, dow, dom, months,
 * and years. Using a bigger type will allow representing a wider range
 * of years. Keep in sync with SPANV_MAX and SPANV_MIN. */
/* TODO rename this */
typedef int Spanv;

/* Basically struct tm. */
struct dtime {
	Spanv min;  /* 0-59 */
	Spanv hour; /* 0-23 */
	Spanv dow;  /* 0-6, <0 => unknown */
	Spanv dom;  /* 0-30 */
	Spanv mon;  /* 0-11 */
	Spanv year; /* ..., -1 == 2BC, 0 == 1BC, 1 == 1AD, 2 == 2AD, ... */
};

struct span {
	Spanv begin;
	Spanv end;
};

struct spanarr {
	struct span *spans;
	size_t len;
	size_t cap;
};

struct entry {
	char *text;
	struct spanarr min, hour, dow, dom, mon, year;
	long dur;
	struct entry *next;
};

struct entryiter {
	struct entry *e;
	struct dtime dt;
	size_t mini, houri, domi, moni, yeari;
	bool dow[7];
	bool skip; /* Set when two or more iters refer to the same event. */
};

/* sres.c */
void entry_init(struct entry *e);
bool entryiter_init(struct entryiter *ei, struct dtime *begin);
bool entryiter_next(struct entryiter *ei);
bool entryiter_stabilizedmy(struct entryiter *ei);
bool entryiter_eq(struct entryiter *a, struct entryiter *b);
void entryiter_sort(struct entryiter *eis, size_t len);
void entryiter_resort(struct entryiter *eis, size_t len);
void spanarr_init(struct spanarr *arr);
bool spanarr_insert(struct spanarr *arr, struct span span);
void spaniter_zero(struct spanarr *arr, Spanv *val, size_t *idx);
bool spaniter_seek(struct spanarr *arr, Spanv *val, size_t *idx, Spanv target);
bool spaniter_next(struct spanarr *arr, Spanv *val, size_t *idx);
bool span_try_merge(struct span *a, struct span *b);

/* parse.c */
char *nexttok(char **s, char delim);
void skipws(char **s);
void stripws(char **s);
bool parse_num(Spanv *n, char **s);
bool parse_min(Spanv *min, char **s);
bool parse_hour(Spanv *hour, char **s);
bool parse_dow(Spanv *dow, char **s);
bool parse_dom(Spanv *dom, char **s);
bool parse_mon(Spanv *mon, char **s);
bool parse_year(Spanv *year, char **s);
bool parse_span(struct span *span, char *s, bool (*str2num)(Spanv*, char**));
bool parse_spanarr(struct spanarr *arr, char *s,
                   bool (*str2num)(Spanv*, char**),
                   Spanv min, Spanv max);
bool parse_duration(long *dur, char **s);
bool parse_entries(struct entry **entry, size_t *n);
bool parse_instant(struct dtime *dt, char *s);

/* output.c */
bool entryiter_printf(char *fmt, struct entryiter *ei);

/* time.c */
bool dtime_isdmyvalid(struct dtime *dt);
bool dtime_calcdow(struct dtime *dt);
int dtime2doy(struct dtime *dt);
int dtime_cmp(struct dtime *a, struct dtime *b);
bool dtime2tm(struct tm *tm, struct dtime *dt);
bool dtime2min(long long *min, struct dtime *dt);
bool min2dtime(struct dtime *dt, long long min);
bool dtime_add(struct dtime *dt, long mins);

/* util.c */
void *malloc_or_exit(size_t n);
void *realloc_or_exit(void *ptr, size_t n);
void errexit(char const *msg);
void errset(char const *s);
void erradd(char const *s);
char *errget(void);
