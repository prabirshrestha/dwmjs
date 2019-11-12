include config.mk

SRC = duktape.c dwmjs.c
OBJ = ${SRC:.c=.o}

all: options dwmjs

options:
	@echo build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

dwmjs: ${OBJ}
	@echo CC -o $@ ${OBJ} ${LDFLAGS}
	@${CC} -o $@${EXE} ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f dwmjs.exe ${OBJ}

.PHONY: all options clean
