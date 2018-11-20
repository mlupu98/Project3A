.SILENT:

default:
	gcc -Wall -std=c99 -Wextra -o lab3a lab3a.c

clean:

dist:
	tar -czvf lab3a-904756085.tar.gz Makefile README lab3a.c ext2_fs.h