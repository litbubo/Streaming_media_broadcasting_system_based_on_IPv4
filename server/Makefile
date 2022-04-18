CFLAGS+=-Wall -g -I../include
LDFLAGS+=-pthread
CC:=gcc
OBJ:=server.o medialib.o threadpool.o tokenbucket.o list.o channel.o
all : server

server : $(OBJ)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

%*.o : %*.c
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)

clean : 
	rm -rf *.o server