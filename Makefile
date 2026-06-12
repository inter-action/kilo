
kilo: kilo.c
	$(CC) kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

run:
	./kilo

fmt:
	clang-format -i kilo.c
