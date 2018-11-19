#NAME: Matei Lupu,Matthew Patern0
#EMAIL: mateilupu20@g.ucla.edu,
#ID: 504798407,

.SILENT

default:
    gcc -Wall -Wextra -o lab3a lab3a.c

dist:
    tar -czvf lab3a-504798407.tar.gz lab3a.c ext2_fs.h Makefile README

clean:
    rm -r lab3a *.tar.gz *CSV.csv
