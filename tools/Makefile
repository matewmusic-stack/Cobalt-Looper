CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -g -fsanitize=address,undefined

test: build/test_looper
	./build/test_looper

build/test_looper: src/cobalt_looper.c test/test_looper.c src/cobalt_looper.h
	@mkdir -p build
	$(CC) $(CFLAGS) src/cobalt_looper.c test/test_looper.c -lm -o $@

clean:
	rm -rf build
.PHONY: test clean
