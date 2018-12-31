all: clean daemonize 
build:	daemonize.c
	gcc -c daemonize.c -Wall -pedantic -O2 -std=c99 -D_POSIX_C_SOURCE=2000112L

daemon-test:	daemonize.o
	gcc daemonize.o -o daemonize

clean:
	rm *.o daemonize
