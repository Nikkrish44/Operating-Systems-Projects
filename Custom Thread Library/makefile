CC = gcc
CFLAGS = -g
RM = rm -f

default: all

all: main
	./main

threads.o: threads.c
	$(CC) $(CFLAGS) -c threads.c -o threads.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

main: main.o threads.o
	$(CC) $(CFLAGS) main.o threads.o -o main

clean:
	$(RM) main main.o threads.o
