CC ?= gcc
CFLAGS ?= -Wall
PREFIX ?= /usr/local

SOURCES = sres.c parse.c output.c time.c util.c
HEADERS = sres.h arg.h config.h

sres: $(HEADERS) $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

.PHONY: install
install: sres
	cp sres $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/sres
	cp sres.1 $(PREFIX)/share/man/man1

.PHONY: clean
clean:
	rm sres
