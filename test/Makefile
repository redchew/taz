CC      ?= gcc
CCFLAGS := -g -Wall -std=c99 -Wno-unused $(CFLAGS) #-fsanitize=address -fsanitize=undefined -fsanitize=leak
CCLIBS  := -l m

run: build
	 @ ./build/test_engine
	 @ ./build/test_index
	 @ ./build/test_code
	 @ ./build/test_record
	 @ ./build/test_formatter

build: build/test_engine build/test_index build/test_code build/test_record build/test_formatter

clean:
	- @ rm -r build/

build/test_engine: test_engine.c ../taz_engine.h ../taz_engine.c ../taz_common.h
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_engine.c $(CCLIBS) -o build/test_engine

build/test_index: test_index.c ../taz_engine.h ../taz_engine.c ../taz_index.h ../taz_index.c ../taz_common.h
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_index.c $(CCLIBS) -o build/test_index

build/test_code: test_code.c ../taz_engine.h ../taz_engine.c ../taz_index.h ../taz_index.c  ../taz_code.h ../taz_code.c
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_code.c $(CCLIBS) -o build/test_code

build/test_record: test_record.c ../taz_engine.h ../taz_engine.c ../taz_index.h ../taz_index.c  ../taz_record.h ../taz_record.c
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_record.c $(CCLIBS) -o build/test_record

build/test_formatter: test_formatter.c ../taz_engine.h ../taz_engine.c ../taz_index.h ../taz_index.c  ../taz_record.h ../taz_record.c ../taz_formatter.h ../taz_formatter.c
	@ mkdir -p build/
	@ $(CC) $(CCFLAGS) test_formatter.c $(CCLIBS) -o build/test_formatter
