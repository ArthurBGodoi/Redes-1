CC = gcc
CFLAGS = -Wall -O2

# Objetos comuns
OBJS = proto.o

all: sender receiver

proto.o: proto.c proto.h
	$(CC) $(CFLAGS) -c proto.c -o proto.o

sender: sender.c $(OBJS)
	$(CC) $(CFLAGS) sender.c $(OBJS) -o sender

receiver: receiver.c $(OBJS)
	$(CC) $(CFLAGS) receiver.c $(OBJS) -o receiver

clean:
	rm -f *.o sender receiver
