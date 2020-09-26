SRES(1)                          User Commands                         SRES(1)

NAME
       sres - simple recurring event scheduler

SYNOPSIS
       sres [-f FMT]
       sres [-f FMT] [BEGIN] END

DESCRIPTION
       Take  a description of events over standard input, and then output when
       the events occur between BEGIN and END.  BEGIN  defaults  to  now;  END
       defaults to one day from now.

   Events
       Event descriptions are given to sres over standard input in the follow‐
       ing format:

           MINS HOURS DAYS-OF-WEEK DAYS-OF-MONTH MONTHS YEARS DURATION Event1
           MINS HOURS DAYS-OF-WEEK DAYS-OF-MONTH MONTHS YEARS DURATION
           # A line comment: "#" must be the first non-whitespace character.
           MINS HOURS DAYS-OF-WEEK DAYS-OF-MONTH MONTHS YEARS DURATION Event2
           [Etc...]

       Each event is one line starting with seven fields, as shown above.  The
       first  six  fields  describe  the start time of an event in a cron-like
       format, and the last field specifies the duration of the event  associ‐
       ated with the preceding start time description.  Everything on the line
       after the seven fields constitutes the  event  description.   An  event
       with no description has the same description as the previous event, and
       events defined in this way are  deduplicated.   For  example,  "Event1"
       above  has  two lines describing when and for how long it occurs.  (The
       very first event in the input must have a description.)

       Here are some examples of the first seven fields:

       00 13 tue,fri * jan-apr,sep 1986 3h30m
              At 1:00 p.m. on Tuesdays and Fridays in  January  through  April
              and September in 1986 for 3.5 hours.

       00-29 08 * 29 feb 2020 -1m1w
              At  each  of  the  first  30 minutes in the 8th hour of 29 Febu‐
              rary 2020 for 1 minute less than a week.

       * * * * * 2030 0m
              At every minute in 2030 for 0 minutes.

       54 18 mon 1 apr 2001 2d
              At 6:54 p.m. on Monday,  1 April 2001  for  2 days.   (This  one
              never happens since 1 Apr 2001 is actually a Sunday.)

       To  be  precise, the permissible values for the first six fields are as
       follows:

       Field           Values
       ──────────────────────────────────────────────────
       mins            0, 1, ..., 59
       hours           0, 1, ..., 23
       days-of-week    sun, mon, tue, wed, thu, fri, sat
       days-of-month   1, 2, ..., 31
       months          jan, feb, mar, apr, may, jun,
                       jul, aug, sep, oct, nov, dec
       years           ..., 2BC, 1BC, 1AD, 2AD, ...

       Days of week and months are case insensitive.  When a year doesn't have
       an era suffix (BC/AD), AD is assumed.

       In  addition,  the  above values can be listed (using ",") and put in a
       range (using "-").

       Durations are specified as a string of one or  more  undelimited  inte‐
       ger/unit  pair  components.  The value of the whole duration is the sum
       of the durations represented by each component.  The valid units are  m
       (minute),  h  (hour),  d  (day), and w (week).  While components may be
       negative, the resulting duration must be nonnegative.

   Timespan Format (Begin and End Arguments)
       The format of both BEGIN and END is  [TIME]/[DATE][(+|-)OFFSET],  where
       TIME  is [HOUR[:MIN][am|pm]], HOUR is [DOM][MON[YEAR]], and OFFSET is a
       duration (same format as described above).  (The  square  brackets  are
       not  required  or valid; they are used here to denote that the contents
       are optional.)

       If TIME is omitted, it  defaults  to  now.   If  DATE  is  omitted,  it
       defaults  to today.  If HOUR is provided, MIN defaults to 0.  If DOM is
       provided, MON and YEAR default to their current values.  If MON is pro‐
       vided,  DOM  defaults  to 0 and YEAR defaults to its current value.  In
       all cases, OFFSET defaults to 0m.

       In particular, "/" means now and "0/" means the beginning of today.

   Output Format (-f Option)
       sres prints out event occurrences separated by  newlines.   Each  event
       occurrence  is displayed according to a format specified using the fol‐
       lowing printf-style conversion specifiers:

              %%     a '%' character

              %n     a newline character

              %t     a tab character

              %x     text associated with event

              %d     duration in minutes

              %b_    _ corresponding to the beginning of the event

              %e_    _ corresponding to the end of the event

       Valid substitutions for _ in %b_ and %e_:

              m      minutes (00-59)

              h      hours (01-12)

              H      hours (00-23)

              p      a.m. or p.m.

              d      day of week (Sunday, Monday, Tuesday, etc.)

              D      day of month (01-31)

              M      month (January, February, March, etc.)

              y      integer year (..., -1 => 2BC, 0 => 1BC, 1 => 1AD, ...)

              Y      positive integer year (1 means 1BC or 1AD, etc.)

              e      era (b.c. or a.d.)

              u      Unix timestamp

       Valid prefix modifiers for _ in %b_ and %e_:

              0      zero-based numeric

                     0d     "Sunday" -> 0, "Monday" -> 1, etc.

                     0M     "January" -> 00, "February" -> 01, etc.

              1      one-based numeric

                     1d     "Sunday" -> 1, "Monday" -> 2, etc.

                     1M     "January" -> 01, "February" -> 02, etc.

              b      change leading zeros to blanks (spaces)

              c      change case

                     cp     "a.m." -> "A.M.", "p.m." -> "P.M."

                     cd     "Sunday" -> "sunday", "Monday" -> "monday", etc.

                     cM     "January" -> "january", "February" ->  "february",
                            etc.

                     ce     "b.c." -> "B.C.", "a.d." -> "A.D."

              s      short

                     sm,    drops leading zero

                     sp     "a.m." -> "am", "p.m." -> "pm"

                     sd     "Sunday" -> "Sun", "Monday" -> "Mon", etc.

                     sM     "January" -> "Jan", "February" -> "Feb", etc.

                     sy     drops  century  number  (1986  ->  86, 2009 -> 09,
                            etc.)

                     se     "b.c." -> "bc", "a.d." -> "ad"

       The prefix modifiers can be combined (in an arbitrary  order)  and  the
       effect is probably what you expect.  Invalid modifiers are ignored.

sres                              2020-07-13                           SRES(1)
