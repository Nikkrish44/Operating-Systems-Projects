CC = gcc
CFLAGS = -g
RM = rm -f

default: all

all: main
    ./main

fs.o: fs.c fs.h
    $(CC) $(CFLAGS) -c fs.c -o fs.o

disk.o: disk.c disk.h
    $(CC) $(CFLAGS) -c disk.c -o disk.o

main.o: main.c fs.h disk.h
    $(CC) $(CFLAGS) -c main.c -o main.o

main: main.o fs.o disk.o
    $(CC) $(CFLAGS) main.o fs.o disk.o -o main

clean:
    $(RM) *.o main
