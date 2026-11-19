# Makefile - builds the Tricora C compiler with one command:  make
#
# Requires gcc, flex and bison (works with Bison 2.4+ and GNU Make 3.81+,
# so both MinGW/MSYS and MSYS2 UCRT64 or Linux toolchains are fine).
#
#   make          build build/tricora
#   make test     build, then run every program in tests/ against its .expected file
#   make clean    remove everything generated in build/

CC     := gcc
FLEX   := flex
BISON  := bison
BUILD  := build
ifeq ($(OS),Windows_NT)
EXE := .exe
endif
TARGET := $(BUILD)/tricora$(EXE)

INCLUDES := -Isrc -Isrc/ast -Isrc/symbol_table -Isrc/semantic -Isrc/codegen -I$(BUILD)
CFLAGS   := -std=gnu99 -Wall -g $(INCLUDES)

SOURCES := src/main.c \
           src/ast/ast.c \
           src/symbol_table/symbol_table.c \
           src/semantic/semantic.c \
           src/codegen/codegen.c

GENERATED := $(BUILD)/parser.tab.c $(BUILD)/lex.yy.c
HEADERS   := $(wildcard src/*/*.h)

.PHONY: all test clean

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

# Bison writes parser.tab.c and parser.tab.h (token numbers shared with the lexer).
$(BUILD)/parser.tab.c: src/parser/parser.y | $(BUILD)
	$(BISON) -d -o $(BUILD)/parser.tab.c src/parser/parser.y

$(BUILD)/parser.tab.h: $(BUILD)/parser.tab.c

$(BUILD)/lex.yy.c: src/lexer/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	$(FLEX) -o$@ src/lexer/lexer.l

$(TARGET): $(SOURCES) $(GENERATED) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(GENERATED)

test: $(TARGET)
	sh tests/run_tests.sh

clean:
	rm -rf $(BUILD)
