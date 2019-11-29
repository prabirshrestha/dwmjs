include config.mk

TARGET_EXEC ?= dwmjs${EXE}

SRCS = duktape.c dwmjs.c
OBJS = ${SRCS:.c=.o}

all: options ${TARGET_EXEC}

options:
	@echo build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJS}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

${TARGET_EXEC}: ${OBJS}
	@echo CC -o $@ ${OBJS} ${LDFLAGS}
	@${CC} -o ${TARGET_EXEC} ${OBJS} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f dwmjs.exe ${OBJS}

.PHONY: all options clean
