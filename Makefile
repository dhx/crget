CC=gcc
CFLAGS=-Wall
OBJS=buffer.o connect.o download.o fd.o format_data.o logger.o main.o modem.o output.o xmalloc.o

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: all clean
	
all: crget

# Dependancies
buffer.o: buffer.h xmalloc.h
connect.o: connect.h fd.h modem.h output.h
download.o: connect.h download.h fd.h format_data.h logger.h output.h xmalloc.h
fd.o: buffer.h fd.h modem.h output.h xmalloc.h
format_data.o: format_data.h
logger.o: logger.h fd.h xmalloc.h output.h
main.o: download.h output.h
modem.o: modem.h output.h xmalloc.h
output.o: output.h


crget: $(OBJS)
	$(CC) -o crget $(OBJS)

clean:
	rm -rf crget *.o
