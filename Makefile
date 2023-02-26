PREFIX?=/usr/local

all: scat

scat: scat.c
	$(CC) $(CFLAGS) -o scat scat.c

install:
	cp scat $(PREFIX)/bin

clean:
	rm -f scat
