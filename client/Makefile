
CC=gcc
LINK=gcc
LDFLAGS=-lpthread
CFLAGS=-fPIC -Wall -g

INCLUDE = -I ./

DEPS = \
	jff_config.h \
	jff_sock.h \
	jff_worker.h \
	jff_log.h

client: client.o jff_log.o jff_sock.o jff_worker.o
	$(LINK) -o $@ $(CFLAGS) $^ $(LDFLAGS)

client.o: client.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $^

jff_log.o: jff_log.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o  $@ $^

jff_sock.o: jff_sock.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $^

jff_worker.o: jff_worker.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $^

clean:
	rm -rf client *.o
