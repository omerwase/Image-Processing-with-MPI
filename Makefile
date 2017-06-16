# Makefile for PA3
all:
	mpicc -g -Wall -o pa4.x ppmFile.c pa4.c

clean:
	rm -f pa4.x
