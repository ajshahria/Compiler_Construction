CC := gcc
FLEX := flex
BISON := bison
CFLAGS := -std=c11 -Wall -Wextra -g -Isrc -Isrc/ast -Isrc/semantic -Isrc/symbol_table -Isrc/codegen -Ibuild
BUILD := build
SOURCES := src/main.c src/ast/ast.c src/symbol_table/symbol_table.c src/semantic/semantic.c src/codegen/codegen.c $(BUILD)/parser.tab.c $(BUILD)/lex.yy.c

.PHONY: all clean
all: $(BUILD)/tricora

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/parser.tab.c $(BUILD)/parser.tab.h &: src/parser/parser.y | $(BUILD)
	$(BISON) -Wall -d -o $(BUILD)/parser.tab.c src/parser/parser.y

$(BUILD)/lex.yy.c: src/lexer/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	$(FLEX) -o $@ src/lexer/lexer.l

$(BUILD)/tricora: $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -rf $(BUILD)

