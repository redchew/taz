CC      ?= gcc
CCFLAGS := -g -Wall -std=c99 -Wno-unused $(CFLAGS) -fsanitize=address -fsanitize=undefined -fsanitize=leak

run: build
	 @ ./build/test_engine
	 @ ./build/test_index
	 @ ./build/test_code
	 @ ./build/test_record

build: build/test_engine build/test_index build/test_code build/test_record

clean:
	- @ rm -r build/

build/test_engine: test_engine.c ../taz_engine.h ../taz_engine.c ../taz_common.h
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_engine.c -o build/test_engine

build/test_index: test_index.c ../taz_engine.h ../taz_engine.c ../taz_index.h ../taz_index.c ../taz_common.h
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_index.c -o build/test_index

build/test_code: test_code.c ../taz_engine.h ../taz_engine.c ../taz_index.h ../taz_index.c  ../taz_code.h ../taz_code.c
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_code.c -o build/test_code

build/test_record: test_record.c ../taz_engine.h ../taz_engine.c ../taz_index.h ../taz_index.c  ../taz_record.h ../taz_record.c
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_record.c -o build/test_record

