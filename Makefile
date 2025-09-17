## runj - run commands in parallel
## Copyright 2025 kmx.io <contact@kmx.io>

PROG = runj
PROG_DEBUG = runj_debug

SRCS = runj.c

OBJS = runj.o

OBJS_DEBUG = runj.debug.o

CFLAGS = -O2

CFLAGS_DEBUG = -g -O0

all: build

build: ${PROG}

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

.SUFFIXES: .debug.o

.c.debug.o:
	${CC} ${CFLAGS_DEBUG} -c $< -o $@

debug: ${PROG_DEBUG}

gdb: debug
	gdb ${PROG_DEBUG}

${PROG}: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} ${LDFLAGS} -o ${PROG}

${PROG_DEBUG}: ${OBJS_DEBUG}
	${CC} ${CFLAGS} ${OBJS_DEBUG} ${LDFLAGS} -o ${PROG_DEBUG}
