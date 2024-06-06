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
all: kvsmctl

.c.o:
	${CC} ${INCLUDES} ${CFLAGS} -c $< -o $@

kvsmctl: util/kvsmctl.c $(OBJ)
	${CC} ${INCLUDES} ${CFLAGS} ${LDFLAGS} ${OBJ} $< -o $@

.PHONY: clean
clean:
	rm -rf ${OBJ}
