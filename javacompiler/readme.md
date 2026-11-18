# Compiler Construction Project

This repository is a university course project for Compiler Construction. The goal of this project is to build a simple compiler system capable of compiling C, C++, and Java programs through the main phases of a compiler: lexical analysis, syntax analysis, semantic analysis, intermediate code generation, optimization, and code generation.

## Project Overview

A compiler translates source code written in a high-level programming language into executable or intermediate code. In this project, we aim to understand how compilers work by implementing a simplified version for three widely used languages: C, C++, and Java.

The main objectives of this project are to:

- Understand the compilation process
- Implement lexical and syntactic analysis for multiple languages
- Build semantic checks and symbol tables
- Generate intermediate code for compiled programs
- Develop a simple user interface for compiler interaction
- Apply compiler design concepts in a practical university project

## Core Compiler Phases

### 1. Lexical Analysis
The lexer reads source code and converts it into tokens such as keywords, identifiers, numbers, operators, and symbols.

### 2. Syntax Analysis
The parser verifies whether the token sequence follows the grammar of the language and generates a parse tree or abstract syntax tree (AST).

### 3. Semantic Analysis
This phase checks variable declarations, type compatibility, scope rules, and other language constraints.

### 4. Intermediate Code Generation
The compiler converts the AST into an intermediate representation (IR) to simplify further processing.

### 5. Optimization
The optimization phase reduces redundant operations and improves the efficiency of the generated code.

### 6. Code Generation
The final phase produces target code that can be executed or translated into machine-level instructions.

## Project Scope

This project focuses on creating a simple compiler environment that supports:

- C language compiler
- C++ language compiler
- Java language compiler
- A user interface for input and result display

The system will support basic compilation tasks and demonstrate the core working principles of compiler construction.

## Team Members and Responsibilities

- Abdul Julil Shahria - Responsible for the C++ language compiler and UI
- Saima Suchi - Responsible for the Java language compiler
- Sabika Rahman - Responsible for the C language compiler

## Suggested Development Workflow

1. Define the language grammar
2. Implement the lexer
3. Build the parser
4. Create the AST
5. Add semantic analysis
6. Generate intermediate code
7. Run tests and fix errors
8. Optimize and finalize code generation

## Getting Started

### Prerequisites

- A modern C++/Java/Python environment, depending on the implementation language
- A compiler or interpreter for the selected language
- A build tool such as CMake, Make, or Gradle

### Example

```bash
git clone https://github.com/yourusername/compiler-construction-project.git
cd compiler-construction-project
# build the project
# run the compiler on an input program
```

## Example Compilation Workflow

```text
source code -> lexer -> parser -> AST -> semantic analysis -> IR -> optimization -> code generation -> target code
```

## Learning Goals

This project helps build practical knowledge in:

- Formal languages and grammar
- Tokenization and parsing
- AST design
- Symbol tables
- Type checking and scope resolution
- Code generation concepts
- Compiler error handling
- Multi-language compiler design

## Contribution

This project is developed as part of the Compiler Construction course. Team members can continue improving the lexer, parser, semantic analyzer, and compiler interface as the project expands.

## License

This project is intended for educational and university coursework purposes.

## Notes

This repository is planned as a simple but functional compiler construction project for academic learning and demonstration.
