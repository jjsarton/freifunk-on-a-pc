#CFLAGS=-g  -Wall
CFLAGS=-O3 -s  -Wall
LDFLAGS=
#CC=clang
CC=gcc

OBJ =	bin/mkjson \
	bin/respondd \
	bin/stations \
	bin/calc_mac \
	bin/selectPrefix \
	bin/queryHost \
	bin/gluon-neighbour-info \
	bin/sse-multiplexd \
	bin/sse-multiplex  \
	bin/neighbours-batadv

all: $(OBJ)

bin/mkjson: src/mkjson.c
	$(CC) $(CFLAGS) -DMKJSON -o $@ $< $(LDFLAGS)

bin/respondd: src/mkjson.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bin/callscript: src/callscript.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	chown root:root $@
	chmod 6755 $@

bin/neighbours-batadv: src/neighbours-batadv.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -ljson-c
	
clean:
	@for f in $(OBJ);do if [ -e $$f ];then rm $$f; fi; done

bin/%:src/%.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)


bin/sse-multiplex:  src/sse-multiplex.c src/sse-multiplex.h
bin/sse-multiplexd: src/sse-multiplexd.c src/sse-multiplex.h


install: $(OBJ)
	@cp $(OBJ) ../bin
