CFLAGS+=-Wall -g -I../include
LDFLAGS+=-pthread
CC:=gcc
OBJ:=server.o Medialib.o Threadpool.o Tokenbucket.o List.o
all : server

server : $(OBJ)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

%*.o : %*.c
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)

clean : 
	rm -rf *.o server