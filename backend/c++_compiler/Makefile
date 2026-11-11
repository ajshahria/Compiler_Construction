# Makefile - builds the whole compiler with one command:  make
# Requires: g++, flex, bison (MinGW/MSYS on Windows or any Linux).
CXX      := g++
CXXFLAGS := -std=gnu++11 -Wall -g
SRC_DIRS := src src/common src/ast src/symbol_table src/semantic src/tac \
            src/optimizer src/codegen src/interpreter src/preprocessor
INCLUDES := $(addprefix -I,$(SRC_DIRS)) -Ibuild
BUILD    := build
TARGET   := $(BUILD)/compiler.exe

SOURCES  := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.cpp))
OBJECTS  := $(addprefix $(BUILD)/,$(notdir $(SOURCES:.cpp=.o))) \
            $(BUILD)/parser.tab.o $(BUILD)/lex.yy.o

vpath %.cpp $(SRC_DIRS)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Parser (Bison) and lexer (Flex) are generated into build/
$(BUILD)/parser.tab.cpp $(BUILD)/parser.tab.h: src/parser/parser.y | $(BUILD)
	bison -d --defines=$(BUILD)/parser.tab.h -o $(BUILD)/parser.tab.cpp $<

$(BUILD)/lex.yy.cpp: src/lexer/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	flex -o $@ $<

$(BUILD)/parser.tab.o: $(BUILD)/parser.tab.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/lex.yy.o: $(BUILD)/lex.yy.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: %.cpp $(BUILD)/parser.tab.h | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: all
	bash tests/run_tests.sh

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
