
kilo: kilo.c
	$(CC) -Wno-strict-prototypes kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

run:
	./kilo

clean:
	rm -rf ./kilo

fmt:
	clang-format -i kilo.c
