CC = gcc
CFLAGS = -g
RM = rm -f

default: all

all: myshell
	./myshell

myshell: myshell.c
	$(CC) $(CFLAGS) -Werror -o myshell myshell.c

clean:
	$(RM) myshell
