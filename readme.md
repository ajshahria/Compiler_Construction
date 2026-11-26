# Tricora: a Mini Language Compiler with Flex and Bison

Compiler Construction Lab project · Department of Computer Science and Engineering · Metropolitan University, Bangladesh

**Live demo:** [tricora.onrender.com](https://tricora.onrender.com) (the free server sleeps when idle, so the first load can take about a minute)

Tricora is a compiler front-end with intermediate code generation for the fixed mini language in section 5 of the lab project manual: `int`, `float`, `bool`, `if`/`else`, `while`, `print` and nested blocks. Each of the three team members built one compiler for this language. All three run the full pipeline the manual requires, and each also accepts the language in the style of a real language: C, C++ or Java.

```text
source program
   │
   ▼
Lexical analyzer (Flex)        ──►  tokens                       lexical errors with line numbers
   │
   ▼
Syntax analyzer (Bison)        ──►  Abstract Syntax Tree          syntax errors, error recovery
   │
   ▼
Semantic analyzer + Symbol Table (nested scopes)                  undeclared, redeclared, scope, type errors
   │
   ▼
Intermediate code generator    ──►  Three Address Code (TAC)      the required final output
   │
   ▼  bonus
Optimizer (--opt) · AST graph (--dot) · TAC interpreter (--run) · x86 assembly (--asm)
```

| Compiler | Folder | Executable | Written in | Author | Style it also accepts (bonus) |
|---|---|---|---|---|---|
| **tricora** (C) | [`backend/c-compiler`](backend/c-compiler) | `build/tricora` | C | Sabika Rahman | `#include`, `int main()`, `return`, `printf`, `scanf`, functions, arrays, `for`, `do-while`, `switch`, `continue`, `++`, `+=` |
| **compiler** (C++) | [`backend/c++_compiler`](backend/c++_compiler) | `build/compiler.exe` | C++11 | Abdul Julil Shahria | `#include`, `int main()`, `cout`, `cin`, functions, arrays, `for`, `do-while`, `switch`, `continue`, `?:`, `++`, `+=` |
| **minijavac** (Java) | [`backend/javacompiler`](backend/javacompiler) | `build/minijavac` | C++17 | Saima Suchi | `class` with `main`, `import`, `System.out.println`, `Scanner`, `boolean`, static methods, arrays, `for`, `do-while`, `switch`, `continue`, `++` |

A program written exactly as in the manual compiles with **all three compilers, with no flags**. Each compiler's README explains its own features, options and output in detail.

## Team members

| Member | Responsibility |
|---|---|
| Abdul Julil Shahria | C++ compiler (`backend/c++_compiler`), web interface (`frontend`, `server`), deployment |
| Saima Suchi | Java compiler (`backend/javacompiler`) |
| Sabika Rahman | C compiler (`backend/c-compiler`) |

## Manual checklist

Each requirement of the lab manual and where it is met. Every row holds for all three compilers.

| Manual section | Requirement | Status |
|---|---|---|
| 4.1 | Lexer: keywords, identifiers, constants, operators, delimiters; comments and whitespace discarded; invalid tokens reported with their line | ✅ |
| 4.2 | Unambiguous CFG in Bison, syntax errors with line numbers, error recovery with the `error` token | ✅ (no Bison conflicts) |
| 4.3 | AST with one node per construct, printed as an indented tree | ✅ `--ast` (Graphviz: `--dot`) |
| 4.4 | Symbol table with name, type, scope and declaration line; nested scopes | ✅ `--symtab` |
| 4.5 | Undeclared variable, redeclaration, scope violation, type mismatch, invalid assignment, invalid expression | ✅ all six, with line numbers |
| 4.6 | TAC for arithmetic (with precedence), relational and logical expressions, `if`, `if-else`, `while`, `print` | ✅ `--tac` (the default output) |
| 5 | The fixed language: `int float bool`, `if`/`else`, `while`, `print`, nested blocks, `+ - * / %`, `< > <= >= == !=`, `&& \|\| !` | ✅ the manual's sample (5.5) and TAC example (4.6) give exactly the manual's output |
| 8 | Directory layout: `docs/`, `src/` by phase, `tests/`, `examples/`, `Makefile`, `README.md` | ✅ |
| 14 | Optional bonus features | ✅ every row of the table, see [Bonus features](#bonus-features-manual-section-14) |
| 15 | Tests: a valid program, a lexical error, a syntax error, one program per semantic rule, each with its expected output | ✅ see [Testing](#testing) |

## Features

### Required modules (manual section 4)

| Module | What it does | Where (in each compiler) |
|---|---|---|
| Lexical analyzer | Keywords, identifiers, int/float/bool constants, every operator and delimiter. Discards `//` and `/* */` comments and whitespace. Reports an invalid token with its line number. | `src/lexer/lexer.l` |
| Syntax analyzer | An unambiguous CFG in Bison, with precedence and associativity declarations and no conflicts. Syntax errors carry line numbers. The `error` token gives recovery, so several errors are reported in one run. | `src/parser/parser.y` |
| Abstract Syntax Tree | One node per construct (declaration, assignment, binary and unary operations, if, while, print, block). Printed as an indented tree with `--ast`, or as a Graphviz graph with `--dot`. | `src/ast/` |
| Symbol table | Name, type, scope and declaration line of every identifier, kept as a stack of scopes, so a variable declared in a block is not visible outside it. Printed with `--symtab`. | `src/symbol_table/` |
| Semantic analyzer | Undeclared variable, redeclaration in the same scope, scope violation, type mismatch, invalid assignment, invalid expression (for example `&&` on numbers). | `src/semantic/` |
| Intermediate code | Three Address Code with temporaries `t1, t2, …` and labels `L1, L2, …`: arithmetic with operator precedence, relational and logical expressions, `if`, `if-else` and `while` (with `ifFalse … goto` and `goto`), and `print`. | C: `src/codegen/` · C++: `src/tac/` · Java: `src/codegen/` |

### Bonus features (manual section 14)

Every feature in the manual's section 14 table is implemented in all three compilers:

| Feature (section 14) | C | C++ | Java |
|---|---|---|---|
| Arrays | `int a[5];` `int b[] = {1, 2};` `a[i]` | `int a[5];` `int b[] = {1, 2};` `a[i]`, `cin >> a[i]` | `int[] a = new int[5];` `int[] b = {1, 2};` `a[i]`, `a.length` |
| Functions, calls, parameters, `return` | `int f(int x) { ... }`, `void g()`, recursion | `int f(int x) { ... }`, `void g()`, recursion | `static int f(int x) { ... }`, `static void g()`, recursion |
| `for` loop | ✅ | ✅ | ✅ |
| `do-while` loop | ✅ | ✅ | ✅ |
| `switch-case` (with `break`, `default`) | ✅ | ✅ | ✅ |
| Unary operators, `++`/`--` | ✅ (also `+= -= *= /= %=`, and `continue`) | ✅ (also `+= -= *= /=`, `continue`, `?:`) | ✅ (also `continue`) |
| Constant folding (`--opt`) | ✅ | ✅ | ✅ |
| Dead code elimination (`--opt`) | ✅ | ✅ | ✅ |
| AST visualization with Graphviz (`--dot`) | ✅ | ✅ | ✅ |
| Improved error recovery | ✅ | ✅ | ✅ |
| Graphical user interface | ✅ one web IDE for all three compilers | ✅ | ✅ |

In the TAC, an array element is read with `t1 = a[i]` and written with `a[i] = t1` (the index counts elements). A function is a `func f:` … `endfunc` block whose parameters are taken with `formal x`; a call passes each argument with `param x`, then `t2 = call f 2`. A call saves the called function's own variables, so recursion works in `--run` and in the `--asm` output.

- **Language extensions** beyond that table: C++ `?:` and `+= -= *= /=`, C `+= -= *= /= %=`, `continue` in all three.
- **Real-language program forms.** C: `#include <stdio.h>`, `int main()` and other functions, `return`, type-checked `printf`. C++: `#include <iostream>`, `int main()` and other functions, `cout`. Java: a `class` with `main` and static methods, `System.out.println`, `import`.
- **Program input** in all three: C `scanf`, C++ `cin >>`, Java `Scanner` (`nextInt`, `nextFloat`, `nextDouble`, `nextBoolean`). Each read becomes the TAC instruction `read x`.
- **Headers phase** in all three (`--headers`): the `#include` headers (C, C++) or `import` lines (Java), with their line numbers.
- **Optimization** (`--opt`): constant propagation and folding, dead and unreachable code elimination, removal of unused temporaries.
- **AST visualization**: `--dot` prints the AST as a Graphviz graph (`dot -Tpng ast.dot -o ast.png`), and the web interface draws the AST as a tree diagram.
- **Improved error recovery**: errors in several statements are reported in one run, with hints such as "missing ';' at the end of line 5?".
- **Graphical user interface**: a browser IDE for all three compilers, which also works on phones (see [Web interface](#web-interface)).
- **Beyond the manual's scope** (not graded): a TAC interpreter (`--run`) and 32-bit x86 assembly output (`--asm`).

## Language summary

The authoritative description is section 5 of the manual. The formal CFG of each compiler is in its grammar file: [C](backend/c-compiler/docs/grammar.md), [C++](backend/c++_compiler/docs/grammar.md), [Java](backend/javacompiler/docs/language-grammar.md).

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
| Bonus (section 14) | arrays, functions with parameters and `return`, `for`, `do-while`, `switch`, `break`, `continue`, `++`/`--` |

## Requirements

| Tool | Used for | Tested with |
|---|---|---|
| GCC / G++ | compiling the compilers (C99 for C, C++11 for the C++ compiler, C++17 for the Java compiler) | MinGW GCC 6.3, GCC on Ubuntu |
| Flex | lexer generator | 2.5.35 |
| Bison | parser generator (2.4 or newer) | 2.4.2 |
| GNU Make | build automation | 3.81 |
| Python 3 and Node.js | only for the web interface | Python 3.12 and Node 20 (Docker), Python 3.14 and Node 24 (Windows) |

On Ubuntu/Debian: `sudo apt install build-essential flex bison`. On Windows, use MinGW/MSYS or MSYS2 and run the commands from its shell or Git Bash.

## Compilation instructions

Each compiler builds with one `make` in its own folder:

```sh
git clone https://github.com/ajshahria/Compiler_Construction.git
cd Compiler_Construction

make -C backend/c-compiler        # -> backend/c-compiler/build/tricora
make -C backend/c++_compiler      # -> backend/c++_compiler/build/compiler.exe
make -C backend/javacompiler      # -> backend/javacompiler/build/minijavac
```

On Windows the C and Java executables get a `.exe` suffix (`tricora.exe`, `minijavac.exe`). `make clean` in a folder deletes its `build/` directory.

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
| `--headers` | the `#include` headers (C, C++) or `import` lines (Java) (bonus) |
| `--tokens` | the token stream (lexical analysis) |
| `--ast` | the Abstract Syntax Tree, indented |
| `--dot` | the Abstract Syntax Tree as a Graphviz graph (bonus); render it with `dot -Tpng ast.dot -o ast.png` |
| `--symtab` | the symbol table: name, type, scope, line declared |
| `--tac` | Three Address Code (the default) |
| `--opt` | optimized TAC (bonus) |
| `--run` | runs the program and prints its output; input comes from standard input (bonus) |
| `--asm` | x86 assembly (bonus) |
| `--all` | every phase above |
| `-o file` | writes the output to a file (the C++ compiler writes the assembly) |

The C++ and Java compilers also take `--strict`, which requires the full C++ form (`#include <iostream>` and `int main()`) or Java form (`class Name { public static void main(String[] args) { … } }`) instead of the manual's plain list of statements.

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

With `--run`, it prints `55`.

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

Every compiler has a `tests/` folder with the four categories required by section 15 of the manual. Each test program `name.<ext>` is paired with `name.expected`, which holds its exact expected output.

| Category | C | C++ | Java | Demonstrates |
|---|---|---|---|---|
| `valid/` | 23 | 25 | 22 | programs that compile cleanly to TAC, including the manual's sample (5.5) and TAC example (4.6), programs that read input, loops and switch, arrays and recursive functions, and the `--dot` graph |
| `lexical/` | 5 | 7 | 5 | invalid characters, invalid identifiers, unterminated comments and strings, unknown headers or imports |
| `syntax/` | 4 | 9 | 5 | missing semicolons and parentheses, unbalanced braces, a broken `for` / `do-while`, a class without `main`, several errors in one file |
| `semantic/` | 14 | 10 | 11 | all six rules of section 4.5, plus printf/scanf, Scanner and initialization checks, and the rules for arrays, functions, `break`, `continue` and `switch` |

Run them all:

```sh
make -C backend/c-compiler test
make -C backend/c++_compiler test
make -C backend/javacompiler test
```

Current result: **C 46/46, C++ 51/51, Java 43/43 passing.** The Docker build runs all three suites, so a deployment fails if any test fails.

## Web interface

A browser IDE for all three compilers (bonus: GUI). Try it at [tricora.onrender.com](https://tricora.onrender.com).

- **Editor (left):**
  - a code editor with a **C / C++ / Java** switch, Open and Save buttons, and **▶ Compile** (or Ctrl+Enter)
  - errors are underlined in red on their line
  - your code is saved separately for each language in the browser
- **Examples ▾:** seven ready-made programs per language: the manual's sample, a full program, reading input, constant folding, and one program each with lexical, syntax and semantic errors. Picking one only loads it; it runs when you press Compile.
- **Output (right):** one tab per phase, grouped as *front end* (Headers, Tokens, AST, Symbol Table, TAC), *back end* (Optimized TAC, Assembly, Output) and *Errors*.
  - The **AST** tab draws the tree as a diagram: coloured by node kind, foldable, zoomable. Clicking a node jumps to its line.
  - The **Optimized TAC** tab can show the TAC and the optimized TAC side by side, with the number of instructions saved.
  - When the program reads input, an input box appears under Output.
- **Pipeline:** the top bar shows Lexer → Parser → Semantic → TAC → Run, which turn green or red after Compile and show where compilation stopped.
- **Projector mode:** a light theme and A−/A+ font size buttons.
- **Wrong-language notice:** if the code looks like another language (for example C code with Java selected), a bar offers to move it there.
- **Phones:** one pane at a time, switched by a Code / Compile / Output bar at the bottom.

### Run it locally

Build the compilers first (see [Compilation instructions](#compilation-instructions)), then:

```sh
# terminal 1: API server on http://localhost:8000
cd server
python -m venv .venv
.venv/Scripts/python -m pip install -r requirements.txt      # Linux/macOS: .venv/bin/python
.venv/Scripts/python -m uvicorn main:app --port 8000

# terminal 2: the editor on http://localhost:5173
cd frontend
npm install
npm run dev
```

After `npm run build` (which writes `frontend/dist/`), the server also serves the editor itself at http://localhost:8000, so only terminal 1 is needed.

### Docker and deployment

One command builds the three compilers and the web interface and runs every test suite:

```sh
docker build -t tricora .
docker run -p 8000:8000 tricora          # then open http://localhost:8000
```

On [Render](https://render.com), **New → Blueprint** with this repository uses `render.yaml` to deploy the same image. The live site redeploys automatically from the `main` branch.

## Directory structure

```text
Compiler_Construction/
├── readme.md                     this file
├── Dockerfile                    builds the compilers and the web interface, runs every test suite
├── render.yaml                   deployment of the Dockerfile on Render
│
├── backend/                      the three compilers; each one is self-contained
│   ├── c-compiler/               "tricora" (Sabika Rahman)
│   │   ├── Makefile              make, make test, make clean
│   │   ├── README.md             features, options, output and tests
│   │   ├── docs/                 grammar.md (formal CFG), PROJECT_REPORT.md
│   │   ├── src/
│   │   │   ├── main.c            driver: runs the phases in order
│   │   │   ├── lexer/            lexer.l (Flex)
│   │   │   ├── parser/           parser.y (Bison), builds the AST
│   │   │   ├── ast/              AST nodes and printer
│   │   │   ├── symbol_table/     scope stack: insert, lookup, enter and leave scope
│   │   │   ├── semantic/         type and scope checking
│   │   │   ├── codegen/          Three Address Code generation
│   │   │   └── backend/          bonus: optimizer, interpreter, x86 assembly (shared with the Java compiler)
│   │   ├── examples/             valid/ and invalid/ sample programs
│   │   └── tests/                valid/, lexical/, syntax/, semantic/ + run_tests.sh
│   │
│   ├── c++_compiler/             "compiler" (Abdul Julil Shahria)
│   │   ├── Makefile, README.md, docs/grammar.md
│   │   ├── src/
│   │   │   ├── main.cpp          driver
│   │   │   ├── preprocessor/     #include handling
│   │   │   ├── lexer/  parser/  ast/  symbol_table/  semantic/
│   │   │   ├── tac/              Three Address Code generation
│   │   │   ├── optimizer/        bonus: TAC optimizer
│   │   │   ├── codegen/          bonus: x86 assembly
│   │   │   ├── interpreter/      bonus: TAC interpreter
│   │   │   └── common/           shared types and error reporting
│   │   ├── examples/             sample programs
│   │   └── tests/                valid/, lexical/, syntax/, semantic/ + run_tests.sh
│   │
│   └── javacompiler/             "minijavac" (Saima Suchi)
│       ├── Makefile, README.md
│       ├── docs/                 language-grammar.md, project-report.md
│       ├── src/
│       │   ├── main.cpp          driver
│       │   ├── lexer/  parser/  ast/  symbol_table/  semantic/
│       │   ├── imports/          import lines
│       │   ├── codegen/          Three Address Code generation
│       │   ├── backend/          bonus: optimizer, interpreter, x86 assembly
│       │   └── common/           shared types and error counters
│       ├── examples/             Example.java
│       └── tests/                valid/, lexical/, syntax/, semantic/ + run_tests.sh
│
├── server/                       web back end (FastAPI): runs a compiler once per phase
│   ├── main.py                   HTTP API: /api/languages, /api/compile
│   ├── compilers.py              the compilers, their phases and options
│   └── requirements.txt
│
└── frontend/                     web front end (React, Vite, Monaco editor)
    ├── src/
    │   ├── App.tsx               layout, state, compiling
    │   ├── EditorPane.tsx        editor, toolbar, Examples menu
    │   ├── OutputPane.tsx        phase tabs, errors, compare view
    │   ├── AstTree.tsx           AST diagram
    │   ├── Pipeline.tsx          phase status in the top bar
    │   ├── examples.ts           the Examples menu
    │   ├── examples/             the example programs (c/, cpp/, java/)
    │   └── styles.css            themes (dark and light) and the phone layout
    └── public/                   logo and icons
```

Each compiler folder follows the layout suggested in section 8 of the manual: `src/` split by compiler phase, `docs/` for the grammar and the report, `examples/` for sample programs, `tests/` for valid and invalid programs with their expected output, and a `Makefile` that builds everything with one `make`. Generated files (`lex.yy.c`, `parser.tab.c`, the executables) go to each compiler's `build/` folder, which is listed in `.gitignore` and not committed.

## Documentation

| Document | Location |
|---|---|
| Per-compiler guides | [C](backend/c-compiler/README.md), [C++](backend/c++_compiler/README.md), [Java](backend/javacompiler/README.md) |
| Formal grammars | [C](backend/c-compiler/docs/grammar.md), [C++](backend/c++_compiler/docs/grammar.md), [Java](backend/javacompiler/docs/language-grammar.md) |
| Project reports | [C](backend/c-compiler/docs/PROJECT_REPORT.md), [Java](backend/javacompiler/docs/project-report.md) |

## License

Coursework for the Compiler Construction Lab. Intended for educational use only.
