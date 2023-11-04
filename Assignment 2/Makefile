#!/usr/bin/make -f
.phony all:
all: acs

acs: ACS.c
	gcc -Wall ACS.c -o ACS -g -pthread

.PHONY clean:
clean:
	-rm -rf *.o *.exe
