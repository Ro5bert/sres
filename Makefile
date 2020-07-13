
CC ?= gcc
CFLAGS ?= -Wall

SOURCES = sres.c parse.c output.c util.c

sres: sres.h $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

.PHONY: clean
clean:
	rm sres
