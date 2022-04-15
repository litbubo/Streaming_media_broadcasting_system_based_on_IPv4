CFLAGS+=-Wall -g -I../include
CC=arm-linux-gnueabihf-gcc
all : client

client : client.o
	$(CC) $^ -o $@ $(CFLAGS)
	rm -rf *.o

clean : 
	rm -rf *.o client