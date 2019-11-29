VERSION = alpha

# Customize below to fit your system

# flags
# CFLAGS = -target x86_64-pc-windows-gnu -std=c99 -pedantic -Wall -Os -DVERSION=\${VERSION}\"
# LDFLAGS = -target x86_64-pc-windows-gnu -mwindows -s
CFLAGS = -target x86_64-windows-msvc -std=c99 -pedantic -Wall -Os -DVERSION=\"${VERSION}\"
LDFLAGS = -target x86_64-windows-msvc

ifeq ($(OS),Windows_NT)
	EXE=.exe
endif

# compiler and linker
CC = clang
