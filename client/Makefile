CFLAGS+=-Wall -g -I../include
CC=gcc
all : client

client : client.o
	$(CC) $^ -o $@ $(CFLAGS)


clean : 
	rm -rf *.o client