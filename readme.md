# Tricora: a Mini Language Compiler with Flex and Bison

Compiler Construction Lab project, Department of Computer Science and Engineering, Metropolitan University, Bangladesh.

Tricora is a compiler front-end with intermediate code generation for the fixed mini language defined in section 5 of the lab project manual (`int`, `float`, `bool`, `if`/`else`, `while`, `print`, nested blocks). It runs every required phase of the manual:

```text
source program
   │
   ▼
Lexical analyzer (Flex)  ──►  tokens               lexical errors with line numbers
   │
   ▼
Syntax analyzer (Bison)  ──►  Abstract Syntax Tree  syntax errors, error recovery
   │
   ▼
Semantic analyzer + Symbol Table (nested scopes)    undeclared, redeclared, scope, type errors
   │
   ▼
Intermediate code generator  ──►  Three Address Code (TAC): the required final output
```

Each team member built one compiler for this same language. All three share the same pipeline. Each one also accepts the language in a different surface style (plain C, C++ or Java), which is how the project goes beyond the fixed specification:

| Compiler | Folder | Executable | Written in | Extra surface syntax (bonus) |
|---|---|---|---|---|
| **tricora** (C) | [`backend/c-compiler`](backend/c-compiler) | `build/tricora` | C | `#include`, `int main()`, `return`, `printf` with `%d %i %f %.Nf %g`, `scanf` |
| **compiler** (C++) | [`backend/c++_compiler`](backend/c++_compiler) | `build/compiler.exe` | C++ | `#include`, `int main()`, `cout`, `cin`, `for`, `do-while`, `switch`, `?:`, `++`, `+=` |
| **minijavac** (Java) | [`backend/javacompiler`](backend/javacompiler) | `build/minijavac` | C++ | `class` with `main`, `import`, `System.out.println`, `Scanner` input, `boolean`, `for`, `do-while`, `switch`, `++` |

A program written exactly as in the manual compiles with **all three compilers, with no flags**.

## Team members

| Member | Responsibility |
|---|---|
| Abdul Julil Shahria | C++ compiler (`backend/c++_compiler`), web user interface (`frontend`, `server`) |
| Saima Suchi | Java compiler (`backend/javacompiler`) |
| Sabika Rahman | C compiler (`backend/c-compiler`) |

## Features

### Required modules (manual section 4)

| Module | What it does | Files (in each compiler) |
|---|---|---|
| Lexical analyzer | Keywords, identifiers, integer/float/boolean constants, all operators and delimiters. Discards `//` and `/* */` comments and whitespace. Reports an invalid token with its line number. | `src/lexer/lexer.l` |
| Syntax analyzer | Unambiguous CFG implemented in Bison, with precedence and associativity declarations and no conflicts. Syntax errors carry line numbers. The `error` token is used for recovery, so several errors are reported in one run. | `src/parser/parser.y` |
| Abstract Syntax Tree | One node per construct (declaration, assignment, binary/unary operation, if, while, print, block). Printed as an indented tree with `--ast`. | `src/ast/` |
| Symbol table | Name, type, scope and declaration line of every identifier. A stack of scopes, so a variable declared inside a block is not visible outside it. Printed with `--symtab`. | `src/symbol_table/` |
| Semantic analyzer | Undeclared variable, redeclaration in the same scope, scope violation, type mismatch, invalid assignment, invalid expression (for example `&&` on numbers). | `src/semantic/` |
| Intermediate code | Three Address Code with temporaries `t1, t2, …` and labels `L1, L2, …`. Covers arithmetic with operator precedence, relational and logical expressions, `if`, `if-else`, `while` (with `ifFalse … goto` and `goto`) and `print`. | C: `src/codegen/`, C++: `src/tac/`, Java: `src/codegen/` |

### Bonus features (manual section 14)

- **Language extensions** (C++ and Java compilers): `for`, `do-while`, `switch-case`, `break`/`continue`, unary and increment/decrement operators.
- **C program form** (C compiler): `#include <stdio.h>`, `int main() { ... }`, `return`, `printf` with type-checked `%d %i %f %.Nf %g` conversions, and `scanf("%d %f", &a, &b)` to read input.
- **Program input** (all three compilers): C++ `cin >> x`, C `scanf`, Java `Scanner` (`nextInt`, `nextFloat`, `nextDouble`, `nextBoolean`). Each read becomes the TAC instruction `read x`; `--run` takes the input from standard input, and the web interface shows an input box.
- **Optimization** (`--opt`): constant propagation and constant folding, dead code and unreachable code elimination, removal of unused temporaries.
- **Graphical user interface**: a browser IDE with a source editor and one output tab per phase (see [Web user interface](#web-user-interface)).
- **Improved error recovery**: errors from several statements are reported in one run.
- Beyond the manual's scope, and not graded: TAC interpreter (`--run`) and x86 assembly output (`--asm`).

## Language summary

The authoritative description is section 5 of the manual. The full formal CFG of each compiler is in its grammar file:
- [C grammar](backend/c-compiler/docs/grammar.md)
- [C++ grammar](backend/c++_compiler/docs/grammar.md)
- [Java grammar](backend/javacompiler/docs/language-grammar.md)

| Element | Supported |
|---|---|
| Types | `int`, `float`, `bool` |
| Statements | declaration `int x;`, assignment `x = 5;`, `if`, `if-else`, `while`, `print x;`, nested blocks `{ … }` |
| Arithmetic operators | `+ - * / %` |
| Relational operators | `< > <= >= == !=` |
| Logical operators | `&& \|\| !` |
| Literals | `42`, `3.14`, `true`, `false` |
| Identifiers | a letter or `_`, then letters, digits or `_` |
| Comments | `// line` and `/* block */` |

## Directory structure

```text
Compiler_Construction/
├── readme.md                     this file
├── Dockerfile                    builds the compilers and the web UI, runs every test suite
├── render.yaml                   one-click deployment of the Dockerfile on Render
│
├── backend/                      the three compilers; each one is self-contained
│   ├── c-compiler/               C compiler "tricora" (Sabika Rahman)
│   │   ├── Makefile              make, make test, make clean
│   │   ├── README.md             build, run and option reference
│   │   ├── docs/
│   │   │   ├── grammar.md        formal CFG, precedence table, lexical rules
│   │   │   └── PROJECT_REPORT.md project report
│   │   ├── src/
│   │   │   ├── main.c            driver: runs the phases in order
│   │   │   ├── lexer/            lexer.l (Flex)
│   │   │   ├── parser/           parser.y (Bison), builds the AST
│   │   │   ├── ast/              AST nodes and printer
│   │   │   ├── symbol_table/     scope stack: insert, lookup, enter/exit scope
│   │   │   ├── semantic/         type and scope checking
│   │   │   ├── codegen/          Three Address Code generation
│   │   │   └── backend/          bonus: optimizer, interpreter, x86 assembly
│   │   ├── examples/             valid/ and invalid/ sample programs (.tri)
│   │   └── tests/                valid/, lexical/, syntax/, semantic/ + run_tests.sh
│   │
│   ├── c++_compiler/             C++ compiler "compiler" (Abdul Julil Shahria)
│   │   ├── Makefile
│   │   ├── README.md
│   │   ├── docs/grammar.md
│   │   ├── src/
│   │   │   ├── main.cpp          driver
│   │   │   ├── lexer/            lexer.l (Flex)
│   │   │   ├── parser/           parser.y (Bison)
│   │   │   ├── preprocessor/     #include handling
│   │   │   ├── ast/
│   │   │   ├── symbol_table/
│   │   │   ├── semantic/
│   │   │   ├── tac/              Three Address Code generation
│   │   │   ├── optimizer/        bonus: TAC optimizer
│   │   │   ├── codegen/          bonus: x86 assembly
│   │   │   ├── interpreter/      bonus: TAC interpreter
│   │   │   └── common/           shared types and error reporting
│   │   ├── examples/             sample programs (.cpp)
│   │   └── tests/                valid/, lexical/, syntax/, semantic/ + run_tests.sh
│   │
│   └── javacompiler/             Java compiler "minijavac" (Saima Suchi)
│       ├── Makefile
│       ├── README.md
│       ├── docs/
│       │   ├── language-grammar.md
│       │   └── project-report.md
│       ├── src/
│       │   ├── main.cpp          driver
│       │   ├── lexer/  parser/  ast/  symbol_table/  semantic/
│       │   ├── codegen/          Three Address Code generation
│       │   ├── backend/          bonus: optimizer, interpreter, x86 assembly
│       │   └── common/           shared types and error counters
│       ├── examples/             Example.java
│       └── tests/                valid/, lexical/, syntax/, semantic/ + run_tests.sh
│
├── server/                       web UI back end (FastAPI): runs a compiler once per phase
│   ├── main.py                   HTTP API
│   ├── compilers.py              which compilers, phases and options the UI offers
│   └── requirements.txt
│
└── frontend/                     web UI front end (React + Vite + Monaco editor)
    ├── src/                      App.tsx, EditorPane.tsx, OutputPane.tsx, api.ts, …
    └── public/                   logo and icons
```

Each compiler folder follows the layout suggested in section 8 of the manual:
- `src/` is split by compiler phase.
- `docs/` holds the grammar and the report.
- `examples/` holds sample programs.
- `tests/` holds valid and invalid programs, each with its expected output.
- `Makefile` builds it with one `make`.

The generated files (`lex.yy.c`, `parser.tab.c`, the executables) go to each compiler's `build/` folder. That folder is listed in `.gitignore` and is not committed.

## Requirements

| Tool | Used for | Tested with |
|---|---|---|
| GCC / G++ | compiling the C and C++ sources (C++11 for the C++ compiler, C++17 for the Java compiler) | MinGW GCC 6.3, GCC on Ubuntu |
| Flex | lexer generator | 2.5.35 |
| Bison | parser generator (2.4 or newer) | 2.4.2 |
| GNU Make | build automation | |
| Python 3 and Node.js | only for the optional web user interface | |

On Ubuntu/Debian: `sudo apt install build-essential flex bison`. On Windows, use MinGW/MSYS or MSYS2 and run the commands from its shell (or Git Bash).

## Compilation instructions

Each compiler builds with a single `make` in its own folder:

```sh
git clone https://github.com/ajshahria/Compiler_Construction.git
cd Compiler_Construction

make -C backend/c-compiler        # -> backend/c-compiler/build/tricora
make -C backend/c++_compiler      # -> backend/c++_compiler/build/compiler.exe
make -C backend/javacompiler      # -> backend/javacompiler/build/minijavac
```

On Windows, the C and Java executables get a `.exe` suffix (`tricora.exe`, `minijavac.exe`). `make clean` in a folder removes its `build/` directory.

## Execution instructions

Run a compiler on a source file. The default output is the Three Address Code:

```sh
backend/c-compiler/build/tricora         program.txt
backend/c++_compiler/build/compiler.exe  program.txt
backend/javacompiler/build/minijavac     program.txt
```

Each phase can be shown on its own. These options work in all three compilers:

| Option | Shows |
|---|---|
| `--headers` | the `#include` headers (C, C++) or `import` lines (Java), with their line numbers (bonus) |
| `--tokens` | token stream (lexical analysis) |
| `--ast` | Abstract Syntax Tree, indented |
| `--symtab` | symbol table: name, type, scope, line declared |
| `--tac` | Three Address Code (the default) |
| `--opt` | optimized TAC (bonus) |
| `--run` | runs the TAC and prints the program's output (bonus) |
| `--asm` | x86 assembly (bonus) |
| `--all` | every phase above |
| `-o file` | write the output to a file (C++ compiler: writes the assembly) |

C++ and Java only: `--strict` requires the full C++ form (`#include <iostream>` and `int main()`) or Java form (`class Name { public static void main(String[] args) { … } }`) instead of the manual's plain statement list. The README in each compiler folder lists the rest of its options.

### Example: valid program

The sample program of manual section 5.5:

```c
int x;
int y;
bool flag;

x = 10;
y = 0;
flag = true;

while (x > 0) {
   y = y + x;
   x = x - 1;
}

if (flag == true) {
   print y;
} else {
   print x;
}
```

`backend/c-compiler/build/tricora sample.txt` prints:

```text
x = 10
y = 0
flag = true
L1:
t1 = x > 0
ifFalse t1 goto L2
t2 = y + x
y = t2
t3 = x - 1
x = t3
goto L1
L2:
t4 = flag == true
ifFalse t4 goto L3
print y
goto L4
L3:
print x
L4:
```

### Example: invalid program

```c
int x;
bool b;
b = 5 + 3.2;
print y;
```

```text
line 3: semantic error: invalid assignment: cannot assign a float value to bool variable 'b'
line 4: semantic error: undeclared variable 'y'
Compilation failed: 2 semantic error(s).
```

The exit status is `0` when the program compiles and `1` when it has errors.

## Testing

Every compiler has a `tests/` folder with the four categories required by section 15 of the manual. Each test program `name.<ext>` is paired with `name.expected`, which holds the exact expected output.

| Category | C | C++ | Java | Demonstrates |
|---|---|---|---|---|
| `valid/` | 15 | 23 | 15 | programs that compile cleanly through to TAC, including the manual's sample (5.5) and TAC example (4.6) |
| `lexical/` | 5 | 7 | 3 | invalid characters, invalid identifiers, unterminated comments |
| `syntax/` | 3 | 9 | 4 | missing semicolons, unbalanced braces, several errors in one file |
| `semantic/` | 11 | 9 | 9 | undeclared, redeclaration, scope violation, type mismatch, invalid assignment, invalid expression |

Run them all:

```sh
make -C backend/c-compiler test
make -C backend/c++_compiler test
make -C backend/javacompiler test
```

Current result: C 34/34, C++ 48/48, Java 31/31 passing.

## Web user interface

A browser IDE for all three compilers (bonus: GUI):
- **Left:** a source editor with a **C / C++ / Java** switch, Open and Save buttons, and a Compile button (or Ctrl+Enter).
- **Right:** one tab per phase: Tokens, AST, Symbol Table, TAC, Optimized TAC, Assembly, Output and Errors. Errors are also underlined in the editor.

Build the compilers first (see above), then:

```sh
# terminal 1: API server on http://localhost:8000
cd server
python -m venv .venv
.venv/Scripts/python -m pip install -r requirements.txt     # Linux/macOS: .venv/bin/python
.venv/Scripts/python -m uvicorn main:app --port 8000

# terminal 2: editor UI on http://localhost:5173
cd frontend
npm install
npm run dev
```

After `npm run build` (which writes `frontend/dist/`), the server also serves the UI itself at http://localhost:8000, so only terminal 1 is needed.

With Docker, one command builds the compilers and the UI and runs all test suites:

```sh
docker build -t tricora .
docker run -p 8000:8000 tricora          # then open http://localhost:8000
```

On [Render](https://render.com), **New → Blueprint** with this repository uses `render.yaml` to deploy the same image.

## Documentation

| Document | Location |
|---|---|
| Formal grammars | [C](backend/c-compiler/docs/grammar.md), [C++](backend/c++_compiler/docs/grammar.md), [Java](backend/javacompiler/docs/language-grammar.md) |
| Project reports | [C](backend/c-compiler/docs/PROJECT_REPORT.md), [Java](backend/javacompiler/docs/project-report.md) |
| Per-compiler guides | [C](backend/c-compiler/README.md), [C++](backend/c++_compiler/README.md), [Java](backend/javacompiler/README.md) |

## License

Coursework for the Compiler Construction Lab. Intended for educational use only.
