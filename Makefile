CXX ?= g++
FLEX ?= flex
BISON ?= bison
BUILD_DIR := java_compiler/build
SRC_DIR := java_compiler/src
EXE_SUFFIX := $(if $(filter Windows_NT,$(OS)),.exe,)
PARSER := $(BUILD_DIR)/parser.tab.cpp
PARSER_HEADER := $(BUILD_DIR)/parser.tab.hpp
LEXER := $(BUILD_DIR)/lex.yy.cpp
COMPILER := $(BUILD_DIR)/minijavac$(EXE_SUFFIX)
UI := $(BUILD_DIR)/compiler_ui.exe

CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CPPFLAGS += -I$(SRC_DIR) -I$(BUILD_DIR)

SOURCES := $(SRC_DIR)/main.cpp \
	$(SRC_DIR)/ast/ast.cpp \
	$(SRC_DIR)/semantic/semantic.cpp \
	$(SRC_DIR)/symbol_table/symbol_table.cpp \
	$(SRC_DIR)/codegen/tac.cpp \
	$(PARSER) $(LEXER)

.PHONY: all clean run test ui

all: $(COMPILER)

$(PARSER): $(SRC_DIR)/parser/parser.y | $(BUILD_DIR)
	$(BISON) -d -o $@ --defines=$(PARSER_HEADER) $<

$(LEXER): $(SRC_DIR)/lexer/lexer.l $(PARSER_HEADER) | $(BUILD_DIR)
	$(FLEX) -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

$(COMPILER): $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@

run: all
	$(COMPILER) java_compiler/examples/Example.java

$(UI): java_compiler/ui/main.cpp
	$(CXX) -std=c++17 -Wall -Wextra -O2 -mwindows $< -o $@ -lcomdlg32

ui: all $(UI)
	$(UI)

test: all
	@set -e; \
	for fixture in java_compiler/test/valid/*.java; do \
		echo "[PASS expected] $$fixture"; \
		$(COMPILER) "$$fixture"; \
	done; \
	for fixture in java_compiler/test/syntax_errors/*.java java_compiler/test/semantic_errors/*.java java_compiler/test/lexical_errors/*.java; do \
		echo "[FAIL expected] $$fixture"; \
		if $(COMPILER) "$$fixture"; then \
			echo "Expected compilation failure: $$fixture"; exit 1; \
		fi; \
	done

clean:
	rm -f $(BUILD_DIR)/minijavac $(BUILD_DIR)/minijavac.exe $(UI) output.tac
