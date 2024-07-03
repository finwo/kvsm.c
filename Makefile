SRC:=
INCLUDES?=-Isrc
SRC+=$(wildcard src/*.c)

override CFLAGS?=-Os -Wall

.PHONY: default
default: all

include lib/.dep/config.mk

OBJ:=$(SRC:.c=.o)
OBJ:=$(OBJ:.cc=.o)

.PHONY: all
all: kvsmctl README.md

.c.o:
	${CC} ${INCLUDES} ${CFLAGS} -c $< -o $@

kvsmctl: util/kvsmctl.c $(OBJ)
	${CC} ${INCLUDES} ${CFLAGS} ${LDFLAGS} ${OBJ} $< -o $@

README.md: src/kvsm.h
	stddoc < $< > README.md

.PHONY: clean
clean:
	rm -rf ${OBJ}
