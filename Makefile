DEBUG=1
ifeq (${DEBUG},1)
CFLAGS = -O0 -ggdb
else
CFLAGS = -O3 -march=native
endif

CFLAGS := ${CFLAGS} -Wall -Werror

INCL = ./include
LDFLAGS = -lpthread -L. -lsstm
SRCPATH = ./src

default: libsstm.a
	cc ${CFLAGS} -I${INCL} src/bank.c -o bank ${LDFLAGS}
	cc ${CFLAGS} -I${INCL} src/ll.c -o ll ${LDFLAGS}
	cc ${CFLAGS} -I${INCL} src/myTest.c -o myTest ${LDFLAGS}

clean:
	rm bank *.o src/*.o


$(SRCPATH)/%.o:: $(SRCPATH)/%.c include/sstm.h include/sstm_alloc.h
	cc $(CFLAGS) -I${INCL} -o $@ -c $<

.PHONY: libsstm.a

libsstm.a:	src/sstm.o src/sstm_alloc.o src/array.o
	ar cr libsstm.a src/sstm.o src/sstm_alloc.o src/array.o

