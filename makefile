#
# Student makefile for Cache Lab
# Note: requires a 64-bit x86-64 system 
#
CC = gcc
CFLAGS = -g -Wall -Werror  -m64

all: csim test-trans tracegen
	# Generate a handin tar file each time you compile
	-tar -cvf ${USER}-handin.tar  csim.c trans.c 

format:
	clang-format -style="{IndentWidth: 2}" -i csim.c

csim: csim.c cachelab.c cachelab.h
	$(CC) $(CFLAGS) -o csim csim.c cachelab.c -lm 

cache: csim.c cachelab.c cachelab.h
	$(CC) -o csim csim.c cachelab.c -lm -g -std=c99

memcheck:
	valgrind --leak-check=full ./csim

test-trans: test-trans.c trans.o cachelab.c cachelab.h
	$(CC) $(CFLAGS) -o test-trans test-trans.c cachelab.c trans.o 

tracegen: tracegen.c trans.o cachelab.c
	$(CC) $(CFLAGS) -O0 -o tracegen tracegen.c trans.o cachelab.c

trans.o: trans.c
	$(CC) $(CFLAGS) -O0 -c trans.c

#
# Clean the src dirctory
#
clean:
	rm -rf *.o
	rm -f *.tar
	rm -f csim
	rm -f test-trans tracegen
	rm -f trace.all trace.f*
	rm -f .csim_results .marker
