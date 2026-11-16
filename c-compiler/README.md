# Tricora C Compiler

A small educational compiler for the shared language specified by the lab manual, as the C component of the Tricora group project. It demonstrates Flex lexical analysis, Bison parsing, AST construction, a scoped symbol table, semantic analysis, and three-address intermediate code generation. It deliberately does not generate machine code.

## Requirements

- Windows with VS Code
- [MSYS2](https://www.msys2.org/) UCRT64 environment with GCC, Make, Flex, and Bison
- VS Code C/C++ extension (optional, for editing assistance)

After installing MSYS2, open the **MSYS2 UCRT64** terminal and install the tools with:

```sh
pacman -Syu
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-gcc flex bison
```

If the first `pacman -Syu` update closes the terminal, reopen UCRT64 and run `pacman -Syu` again before installing the packages.

Open the `tricora-c` folder in VS Code. Open a UCRT64 terminal in VS Code (Terminal > New Terminal, choose UCRT64) and run:

```sh
make
./build/tricora examples/valid/control.tri
./build/tricora --tokens examples/valid/control.tri
./build/tricora --ast examples/valid/control.tri
./build/tricora --ir examples/valid/control.tri
```

To emit IR to a file: `./build/tricora --ir examples/valid/control.tri -o output.ir`.

## Language overview

Types: `int`, `float`, and `bool`. Statements: declarations, assignments, blocks, `if`/`else`, `while`, and `print`. Operators include arithmetic (including modulo), relational, equality, and boolean operators. Variables must be declared before use; same-scope redeclaration and invalid type use are errors. Braces create scopes. `//` and `/* ... */` comments are supported. Check `docs/grammar.md` against the exact mini-language specification your instructor gave your group before submission.

The complete grammar is in `docs/grammar.md`. Implementation notes and the required report outline are in `docs/PROJECT_REPORT.md`.

## Project layout

```text
docs/       grammar and report guide
src/        compiler implementation, grouped as in the lab manual
  lexer/    Flex lexical analyzer
  parser/   Bison grammar and parser
  ast/      AST nodes and display
  semantic/ semantic checks
  symbol_table/ scoped symbol table
  codegen/  three-address code generation
  main.c    command-line driver
tests/      valid and invalid compiler inputs
examples/   demo programs
build/      generated parser/lexer and executable (created by make)
```

## GitHub workflow

The lab manual requires **one shared GitHub repository for the three-member group**. Use your existing team repository; if it has not been created yet, coordinate with your teammates so one person creates it. Do not make a separate repository for each language component.

1. Clone the shared repository to your computer (use the HTTPS or SSH URL shown by GitHub):
   `git clone <team-repository-url>`
2. Copy this whole `tricora-c` folder into the cloned repository and, if your team prefers, rename it `c-compiler`.
3. In the UCRT64 terminal, change directory to the cloned repository root and run:

```sh
git status
git add c-compiler/
git commit -m "Add C compiler frontend"
git push
```

If you kept the folder name `tricora-c`, use `git add tricora-c/` instead. Do not commit generated files in `build/`; the `.gitignore` excludes them. Each member should make meaningful commits during development, as the manual requires.

