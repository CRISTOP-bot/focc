# Makefile de FOC.
#
# En POSIX: make
# En Windows: make (si hay GNU make) o build.ps1
#
# La logica de construccion vive en Makefile.def; este fichero solo la
# orquesta por sistema operativo.

ifeq ($(OS),Windows_NT)
MKDIR := if not exist bin mkdir bin
MKDIR_TEST := if not exist build mkdir build
EXE := .exe
CLEAN := -rmdir /S /Q bin build || cd .
else
MKDIR := mkdir -p bin
MKDIR_TEST := mkdir -p build
EXE :=
CLEAN := rm -rf bin build
endif

ifeq ($(OS),Windows_NT)
CC = gcc
else
CC = cc
endif
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Ifoc/include
TEST_CFLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Ifoc/include -Itests
BIN ?= bin/foc$(EXE)
TEST_BIN ?= build/ut_suite.exe

.PHONY: all build test check clean fmt

all: build

include Makefile.def

FOC_TEST_SOURCES := $(filter-out foc/src/driver/main.c,$(FOC_SOURCES))

build:
	$(MKDIR)
	$(CC) $(CFLAGS) $(FOC_SOURCES) -o $(BIN)

test: build
	$(MKDIR_TEST)
	$(CC) $(TEST_CFLAGS) $(TEST_SOURCES) $(FOC_TEST_SOURCES) -o $(TEST_BIN)
	$(TEST_BIN)

check: test

clean:
	$(CLEAN)

fmt:
	@command -v clang-format >/dev/null && clang-format -i $(FOC_SOURCES) $(TEST_SOURCES) || true
