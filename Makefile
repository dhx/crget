CC=gcc
CFLAGS=-g -Wall
OBJS=buffer.o connect.o download.o fd.o format_data.o logger.o main.o modem.o output.o xmalloc.o

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: all clean
	
all: crget

crget: $(OBJS)
	$(CC) -o crget $(OBJS)

clean:
	rm -rf crget *.o
