CC      ?= gcc
CCFLAGS := -Og -g -Wall -std=c99 -fsanitize=address -fsanitize=undefined -fsanitize=leak -Wno-unused $(CFLAGS)

run: build
	@ ./build/test_engine

build: build/test_engine

clean:
	- @ rm -r build/

build/test_engine: test_engine.c ../taz_engine.h ../taz_engine.c ../taz_common.h
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_engine.c -o build/test_engine

