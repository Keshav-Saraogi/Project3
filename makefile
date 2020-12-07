CC = gcc
CFLAGS= -Wall

executables = program2

all: program1 program2

program2: Program2-ThreadBased.c utility.c main.h
	$(CC) $(CFLAGS) -o program2 Program2-ThreadBased.c -I. -pthread

clean :
	rm $(executables)
