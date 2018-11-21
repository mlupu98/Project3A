.SILENT:

default: ext2_fs.h lab3a.c
	gcc -Wall -std=gnu99 -g -lm -Wextra -o lab3a lab3a.c

clean:
	rm -rf lab3a *.tar.gz *.dSYM

dist:
	tar -czvf lab3a-904756085.tar.gz Makefile README lab3a.c ext2_fs.h