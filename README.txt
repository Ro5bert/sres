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

           My Event 1
           @ MINS HOURS DAYS-OF-WEEK DAYS-OF-MONTH MONTHS YEARS DURATION
           # A line comment: "#" must be the first non-whitespace character.
           My Event 2
           @ MINS HOURS DAYS-OF-WEEK DAYS-OF-MONTH MONTHS YEARS DURATION
           Etc...

       Each event has a one line description and zero or more "selectors" (the
       lines  beginning with "@").  Each selector consists of seven fields, as
       shown above.  The first six fields describe the start time of an  event
       in a cron-like format, and the last field specifies the duration of the
       event associated with the preceding start time description.   An  event
       with  zero  selectors  never  happens; an event with multiple selectors
       happens at the times described by each of its  selectors,  with  dupli‐
       cates removed.

       Here are some examples of selectors:

       00 13 tue,fri * jan-apr,sep 1986 3h30m
              At  1:00 p.m.  on  Tuesdays and Fridays in January through April
              and September in 1986 for 3.5 hours.

       00-29 08 * 29 feb 2020 -1m1w
              At each of the first 30 minutes  in  the  8th hour  of  29 Febu‐
              rary 2020 for 1 minute less than a week.

       * * * * * 2030 0m
              At every minute in 2030 for 0 minutes.

       54 18 mon 1 apr 2001 2d
              At  6:54 p.m.  on  Monday,  1 April 2001  for 2 days.  (This one
              never happens since 1 Apr 2001 is actually a Sunday.)

       To be precise, the permissible values for the first six fields  are  as
       follows:

       Field           Values
       ──────────────────────────────────────────────────
       mins            00, 01, ..., 59
       hours           00, 01, ..., 23
       days-of-week    sun, mon, tue, wed, thu, fri, sat
       days-of-month   01, 02, ..., 31
       months          jan, feb, mar, apr, may, jun,
                       jul, aug, sep, oct, nov, dec
       years           1970, 1971, ..., 2037

       Leading  zeros are required where shown above.  Days of week and months
       are case insensitive.  The restriction on years may be  lifted  in  the
       future.

       In  addition,  the  above values can be listed (using ",") and put in a
       range (using "-").

       Durations are specified as a string of one or  more  undelimited  inte‐
       ger/unit  pair  components.  The value of the whole duration is the sum
       of the durations represented by each component.  The valid units are  m
       (minute),  h  (hour),  d  (day), and w (week).  While components may be
       negative, the resulting duration must be nonnegative.

   Timespan Format (Begin and End Arguments)
       The format of both BEGIN and  END  is  [HHmmDDMMMyyyy][+OFFSET].   (The
       square brackets are not required or valid; they are used here to denote
       that the contents are optional.)  Here, HH stands for the hour (00-23),
       mm  stands  for  the  minute  (00-59),  DD  stands for the day-of-month
       (01-31), MMM stands for the abbreviated month name  (jan,  feb,  etc.),
       yyyy  stands for the year (1970-2037), and OFFSET stands for a duration
       offset in the same  format  as  durations  for  events.   For  example,
       164509nov2008+-1w1d means 6 days before 4:45 p.m. on 9 November 2008.

       If the first component of BEGIN/END is omitted, it defaults to the cur‐
       rent time; if the second component is omitted, it defaults to 0m.

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

              y      year (4-digit decimal year)

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

              s      short

                     sm     drops leading zero

                     sh     drops leading zero

                     sH     drops leading zero

                     sp     "a.m." -> "am", "p.m." -> "pm"

                     sd     "Sunday" -> "Sun", "Monday" -> "Mon", etc.

                     sD     drops leading zero

                     sM     "January" -> "Jan", "February" -> "Feb", etc.

                     sy     drops  century  number  (1986  ->  86, 2009 -> 09,
                            etc.)

       The prefix modifiers can be combined (in an arbitrary  order)  and  the
       effect is probably what you expect.  Invalid modifiers are ignored.

sres                              2020-07-13                           SRES(1)
