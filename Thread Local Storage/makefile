CC = gcc
CFLAGS = -g -Wall -Werror
LDFLAGS = -lpthread
RM = rm -f

default: all

all: main
  ./main

tls.o: tls.c
	$(CC) $(CFLAGS) -c tls.c -o tls.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

main: main.o tls.o
	$(CC) $(CFLAGS) main.o tls.o -o main $(LDFLAGS)

clean:
	$(RM) *.o main
