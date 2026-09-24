CC = gcc
CFLAGS = -Wall -pthread

all: server client

server: server.c sudoku.c sudoku.h
	$(CC) $(CFLAGS) -o server server.c sudoku.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client

gui: client_gui.c
	gcc -Wall -pthread -o client_gui client_gui.c \
	    $(shell pkg-config --cflags --libs gtk+-3.0)
