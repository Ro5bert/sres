main: main.c
	@gcc -Wall -o main main.c

.PHONY: clean
clean:
	rm main
