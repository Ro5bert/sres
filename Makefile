CC ?= gcc
CFLAGS ?= -Wall

SOURCES = sres.c parse.c output.c time.c util.c
HEADERS = sres.h arg.h config.h

sres: $(HEADERS) $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

.PHONY: clean
clean:
	rm sres
