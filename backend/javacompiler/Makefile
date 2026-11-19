# Makefile - builds the MiniJava compiler with one command:  make
#
#   make          build build/minijavac
#   make test     build, then run every program in tests/ against its .expected file
#   make run      compile examples/Example.java
#   make ui       build and start the Windows GUI
#   make clean    remove everything generated in build/
#
# Requires g++ (C++17), flex and bison (Bison 2.4 or newer).

CXX   ?= g++
FLEX  ?= flex
BISON ?= bison

BUILD_DIR := build
SRC_DIR   := src
EXE_SUFFIX := $(if $(filter Windows_NT,$(OS)),.exe,)

PARSER        := $(BUILD_DIR)/parser.tab.cpp
PARSER_HEADER := $(BUILD_DIR)/parser.tab.hpp
LEXER         := $(BUILD_DIR)/lex.yy.cpp
COMPILER      := $(BUILD_DIR)/minijavac$(EXE_SUFFIX)
UI            := $(BUILD_DIR)/compiler_ui.exe

CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CPPFLAGS += -I$(SRC_DIR) -I$(BUILD_DIR)

SOURCES := $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/ast/ast.cpp \
           $(SRC_DIR)/semantic/semantic.cpp \
           $(SRC_DIR)/symbol_table/symbol_table.cpp \
           $(SRC_DIR)/codegen/tac.cpp
HEADERS := $(wildcard $(SRC_DIR)/*/*.h)

.PHONY: all clean run test ui

all: $(COMPILER)

$(BUILD_DIR):
	mkdir -p $@

# Bison writes the parser and the token header shared with the lexer.
$(PARSER): $(SRC_DIR)/parser/parser.y | $(BUILD_DIR)
	$(BISON) -d -o $@ --defines=$(PARSER_HEADER) $<

$(PARSER_HEADER): $(PARSER)

$(LEXER): $(SRC_DIR)/lexer/lexer.l $(PARSER_HEADER) | $(BUILD_DIR)
	$(FLEX) -o $@ $<

$(COMPILER): $(SOURCES) $(HEADERS) $(PARSER) $(LEXER)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(PARSER) $(LEXER) -o $@

run: all
	$(COMPILER) --all examples/Example.java

$(UI): ui/main.cpp | $(BUILD_DIR)
	$(CXX) -std=c++17 -Wall -Wextra -O2 -mwindows $< -o $@ -lcomdlg32 -lcomctl32

ui: all $(UI)
	$(UI)

test: all
	sh tests/run_tests.sh

clean:
	rm -rf $(BUILD_DIR) output.tac ast.txt symbols.txt
