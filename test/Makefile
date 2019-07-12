CC      ?= gcc
CCFLAGs := -Wall -std=c99 -fsanitize=address -fsanitize=undefined -fsanitize=leak $(CFLAGS)

test: test_engine

test_engine: test_engine.c ../taz_engine.h
	@ $(CC) $(CCFLAGS) test_engine.c -o test_engine
	@ ./test_engine
	