#!/usr/bin/make -f
.phony all:
all: diskinfo disklist diskget diskput

diskinfo: diskinfo.c
	gcc -Wall -g diskinfo.c -o diskinfo

disklist: disklist.c
	gcc -Wall -g disklist.c -o disklist

diskget: diskget.c
	gcc -Wall -g diskget.c -o diskget

diskput: diskput.c
	gcc -Wall -g diskput.c -o diskput -lm

.PHONY clean:
	-rm -rf *.o *.exe
