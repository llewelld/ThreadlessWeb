CC := gcc
CFLAGS := -O -Wall -Werror -DLINUX
CLIBS :=

SRCS := threadlessweb.c twexample.c
OBJS := ${SRCS:c=o}
PROGS := threadlessweb

.PHONY: all

all: ${PROGS}

${PROGS}: ${OBJS}
	$(CC) $(CLIBS) $^ -o $@

%.o: %.c makefile
	${CC} ${CFLAGS} -c $<

.PHONY: clean

clean:
	rm -f ${PROGS} ${OBJS}

