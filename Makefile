sres: sres.c
	@gcc -Wall -o sres sres.c parse.c output.c util.c

.PHONY: clean
clean:
	rm sres
