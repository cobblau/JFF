
CC=gcc
LINK=gcc
LDFLAGS=-lpthread
CFLAGS=-fPIC -DSHMEM -Wall -g

INCLUDE = -I ./

DEPS = \
	jff_config.h \
	jff_sock.h \
	jff_worker.h \
	jff_ring.h \
	jff_atomic.h \
	jff_log.h

server: server.o jff_log.o jff_sock.o jff_worker.o jff_ring.o
	$(LINK) -o $@ $(CFLAGS) $^ $(LDFLAGS)

server.o: server.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $^

jff_log.o: jff_log.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o  $@ $^

jff_sock.o: jff_sock.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $^

jff_worker.o: jff_worker.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $^

jff_ring.o: jff_ring.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $^

clean:
	rm -rf server *.o
