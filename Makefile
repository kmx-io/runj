## runj - run commands in parallel
## Copyright 2025 kmx.io <contact@kmx.io>

PROG = runj
PROG_DEBUG = runj_debug

VERSION = 0.1

SRCS = runj.c

OBJS = runj.o

OBJS_DEBUG = runj.debug.o

DIST = ${PROG}-${VERSION}

CFLAGS = -W -Wall -Werror -std=c89 -pedantic -O2 -pipe

CFLAGS_DEBUG = -W -Wall -Werror -std=c89 -pedantic -g -O0

CLEANFILES = *.o ${PROG} ${PROG_DEBUG} ${DIST}.tar.gz

prefix ?= /usr/local

bindir = ${prefix}/bin

all: build debug

build: ${PROG}

clean:
	rm -f ${CLEANFILES}
	rm -rf ${DIST}

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

.SUFFIXES: .debug.o

.c.debug.o:
	${CC} ${CFLAGS_DEBUG} -c $< -o $@

debug: ${PROG_DEBUG}

dist: ${DIST}.tar.gz

${DIST}.tar.gz:
	rm -rf ${DIST}.old
	! test -d ${DIST} || mv ${DIST} ${DIST}.old
	mkdir ${DIST}
	cpio -pdl ${DIST} < ${PROG}.index
	tar czf ${DIST}.tar.gz ${DIST}

gdb: debug
	gdb ${PROG_DEBUG}

install:
	install -d -m 0755 ${DESTDIR}${bindir}
	install -m 0755 ${PROG} ${DESTDIR}${bindir}/${PROG}
	install -m 0755 ${PROG_DEBUG} ${DESTDIR}${bindir}/${PROG_DEBUG}

${PROG}: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} ${LDFLAGS} -o ${PROG}

${PROG_DEBUG}: ${OBJS_DEBUG}
	${CC} ${CFLAGS} ${OBJS_DEBUG} ${LDFLAGS} -o ${PROG_DEBUG}

uninstall:
	rm -f ${bindir}/${PROG}
	rm -f ${bindir}/${PROG_DEBUG}

.PHONY: all build clean debug dist ${DIST}.tar.gz gdb
