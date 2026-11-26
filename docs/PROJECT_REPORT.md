# Tricora: Project Report

**Design and Implement a Mini Programming Language Compiler using Flex and Bison**

Compiler Construction Lab · Department of Computer Science and Engineering · Metropolitan University, Bangladesh

| Member | Responsibility |
|---|---|
| Abdul Julil Shahria | C++ compiler (`backend/c++_compiler`), web interface (`frontend/`, `server/`), deployment |
| Saima Suchi | Java compiler (`backend/javacompiler`) |
| Sabika Rahman | C compiler (`backend/c-compiler`) |

Repository: <https://github.com/ajshahria/Compiler_Construction> · Live demo: <https://tricora.onrender.com>

This report follows the chapter structure of section 12 of the lab manual. Places for the screenshots that section 11 asks for are marked **[Screenshot]**.

---

## Contents

1. Introduction
2. Objectives
3. Language Specification
4. Compiler Architecture
5. Lexer Design
6. Parser Design
7. Abstract Syntax Tree
8. Semantic Analysis
9. Symbol Table
10. Intermediate Code
11. Challenges
12. Testing
13. Conclusion
14. References

---

## 1. Introduction

During the semester the phases of a compiler were studied one at a time: lexical analysis, syntax analysis, semantic analysis and intermediate code generation. This project joins them into complete working pipelines for the fixed mini language defined in section 5 of the lab manual: three data types (`int`, `float`, `bool`), declarations, assignments, `if`, `if-else`, `while`, `print` and nested blocks.

Each of the three team members built one compiler for this same language, so the group has three independent front ends that can be compared side by side:

| Compiler | Folder | Executable | Implementation language | Also accepts the language written as |
|---|---|---|---|---|
| **tricora** | `backend/c-compiler` | `build/tricora` | C (C99) | C: `#include`, `int main()`, `printf`, `scanf` |
| **compiler** | `backend/c++_compiler` | `build/compiler.exe` | C++11 | C++: `#include <iostream>`, `int main()`, `cout`, `cin` |
| **minijavac** | `backend/javacompiler` | `build/minijavac` | C++17 | Java: `class` with `main`, `System.out.println`, `Scanner` |

All three are built with Flex and Bison, and all three compile a program written exactly as in the manual with no options. Each one stops at the first phase that finds errors, reports every error of that phase with its line number, and otherwise prints Three Address Code (TAC), the output the manual requires. A shared web interface lets every phase of every compiler be seen in a browser.

The main motivation was the one stated in the manual: to show, through working code, how the phases depend on one another. A line number recorded by the lexer appears in a semantic error three phases later; a precedence decision made in the parser is what makes the TAC evaluate `a + b * 2` correctly; and the scope stack of the symbol table decides both which errors are reported and how variables are named in the TAC.

## 2. Objectives

The project sets out to demonstrate that each compiler:

1. **Tokenises** the language with a Flex lexer: keywords, identifiers, integer, float and boolean constants, every operator and delimiter; discards comments and whitespace; and reports any invalid token as a lexical error with its line number.
2. **Parses** the token stream with a complete, unambiguous context-free grammar in Bison (no shift/reduce or reduce/reduce conflicts), reports syntax errors with line numbers, and recovers from them so that several errors are reported in one run.
3. **Builds an Abstract Syntax Tree** with one node per meaningful construct, and prints it in readable form.
4. **Keeps a symbol table** that records the name, type, scope and declaration line of every identifier and supports nested scopes.
5. **Checks the semantic rules** of manual section 4.5: undeclared variable, redeclaration, scope violation, type mismatch, invalid assignment and invalid expression.
6. **Generates Three Address Code** for arithmetic, relational and logical expressions, `if`, `if-else`, `while` and `print`, using temporaries, labels and conditional and unconditional jumps.
7. **Is tested** with valid programs and with programs containing lexical, syntax and every kind of semantic error, each paired with its expected output.

Beyond the required modules, the project also implements every optional feature of manual section 14 in all three compilers (chapter 3.4).

## 3. Language Specification

### 3.1 Elements of the language

| Element | Definition |
|---|---|
| Data types | `int` (signed integer), `float` (floating point), `bool` (`true` / `false`) |
| Statements | declaration `int x;` (optionally `int x = e;`), assignment `x = e;`, `if`, `if-else`, `while`, `print e;`, nested blocks `{ ... }` |
| Arithmetic operators | `+ - * / %` and unary `-` |
| Relational operators | `< > <= >= == !=` |
| Logical operators | `&& \|\| !` |
| Delimiters | `{ } ( ) ;` |
| Identifiers | a letter or `_`, followed by letters, digits or `_` |
| Literals | integers (`42`), floats (`3.14`), booleans (`true`, `false`) |
| Comments | `// to end of line` and `/* block */` |

### 3.2 Context-free grammar

The grammar below is the core language shared by all three compilers. `ε` is the empty string; terminals are quoted or written in capitals.

```text
<program>     ::= <stmt-list>
<stmt-list>   ::= <stmt-list> <statement> | ε
<statement>   ::= <type> IDENT ";"
                | <type> IDENT "=" <expr> ";"
                | IDENT "=" <expr> ";"
                | "print" <expr> ";"
                | "if" "(" <expr> ")" <statement>
                | "if" "(" <expr> ")" <statement> "else" <statement>
                | "while" "(" <expr> ")" <statement>
                | <block>
<block>       ::= "{" <stmt-list> "}"
<type>        ::= "int" | "float" | "bool"

<expr>        ::= <expr> "||" <and-expr> | <and-expr>
<and-expr>    ::= <and-expr> "&&" <eq-expr> | <eq-expr>
<eq-expr>     ::= <eq-expr> "==" <rel-expr> | <eq-expr> "!=" <rel-expr> | <rel-expr>
<rel-expr>    ::= <rel-expr> "<" <add-expr> | <rel-expr> ">" <add-expr>
                | <rel-expr> "<=" <add-expr> | <rel-expr> ">=" <add-expr> | <add-expr>
<add-expr>    ::= <add-expr> "+" <mul-expr> | <add-expr> "-" <mul-expr> | <mul-expr>
<mul-expr>    ::= <mul-expr> "*" <unary-expr> | <mul-expr> "/" <unary-expr>
                | <mul-expr> "%" <unary-expr> | <unary-expr>
<unary-expr>  ::= "!" <unary-expr> | "-" <unary-expr> | <primary>
<primary>     ::= INT_LITERAL | FLOAT_LITERAL | "true" | "false" | IDENT | "(" <expr> ")"
```

The layered `<expr>` rules state precedence and associativity in the grammar itself. In the Bison files the same language is written more compactly as one ambiguous `expression` rule plus precedence declarations (chapter 6); both forms accept exactly the same programs.

Each compiler's complete grammar, including its real-language form and the bonus constructs, is in its own grammar file: [C](../backend/c-compiler/docs/grammar.md), [C++](../backend/c++_compiler/docs/grammar.md), [Java](../backend/javacompiler/docs/language-grammar.md).

### 3.3 Typing rules

| Construct | Allowed operands | Result type |
|---|---|---|
| `+ - * /` | int or float | int if both operands are int, otherwise float |
| `%` | int, int (C, C++); int or float (Java) | int (float in Java if either is float) |
| `< > <= >=` | int or float | bool |
| `== !=` | two numbers, or two bools | bool |
| `&& \|\|`, `!` | bool | bool |
| unary `-` | int or float | the operand's type |
| assignment, initializer | the value's type equals the variable's, or an int stored into a float (widening) | |
| `if` / `while` condition | bool | |

### 3.4 Bonus features (manual section 14)

Every optional feature of section 14 is implemented in all three compilers:

| Feature | C | C++ | Java |
|---|---|---|---|
| Arrays | `int a[5];` `int b[] = {1, 2};` | `int a[5];` `cin >> a[i];` | `int[] a = new int[5];` `a.length` |
| Functions, calls, parameters, `return` | `int f(int x) { ... }`, recursion | `int f(int x) { ... }`, recursion | `static int f(int x) { ... }`, recursion |
| `for`, `do-while`, `switch-case` | ✅ | ✅ | ✅ |
| Unary operators, `++` / `--` | ✅ (also `+= -= *= /= %=`) | ✅ (also `+= -= *= /=`, `?:`) | ✅ |
| Constant folding, dead code elimination | `--opt` | `--opt` | `--opt` |
| AST visualization with Graphviz | `--dot` | `--dot` | `--dot` |
| Improved error recovery | ✅ | ✅ | ✅ |
| Graphical user interface | one web IDE for all three | | |

The project also goes beyond section 14 with program input (`scanf`, `cin`, `Scanner`), a TAC interpreter (`--run`) and 32-bit x86 assembly output (`--asm`). Section 6 says these are not required; they were kept because they make it possible to check that the generated TAC really computes the right result.

## 4. Compiler Architecture

### 4.1 Pipeline

```text
 source program
      │
      ▼
 ┌────────────────────────┐   tokens with line numbers
 │ Lexical analyzer       │──────────────────────────┐
 │ src/lexer/lexer.l      │                          │
 └────────────────────────┘                          ▼
                              ┌────────────────────────────────────┐
                              │ Syntax analyzer                    │
                              │ src/parser/parser.y                │
                              │ builds the AST (src/ast/)          │
                              └────────────────────────────────────┘
                                                │ AST
                                                ▼
                              ┌────────────────────────────────────┐
                              │ Semantic analyzer                  │   errors: undeclared,
                              │ src/semantic/                      │   redeclared, scope,
                              │ + symbol table src/symbol_table/   │   type, assignment,
                              └────────────────────────────────────┘   expression
                                                │ annotated AST
                                                ▼
                              ┌────────────────────────────────────┐
                              │ Intermediate code generator        │
                              │ C: src/codegen/  C++: src/tac/     │
                              │ Java: src/codegen/                 │
                              └────────────────────────────────────┘
                                                │ Three Address Code  (required output)
                                                ▼
                              ┌────────────────────────────────────┐
                              │ bonus: optimizer (--opt),          │
                              │ interpreter (--run), x86 (--asm)   │
                              └────────────────────────────────────┘
```

### 4.2 How the modules interact

- **Lexer and parser run together.** Bison's parser calls `yylex()` whenever it needs the next token. The lexer passes each token's value (an identifier's name, a literal's value) in `yylval`, and its line number in `yylloc`, which the parser uses for error messages and stores in every AST node.
- **The parser builds the AST bottom-up.** Each grammar rule's action creates a node from the nodes of its right-hand side, so when the start symbol is reduced the whole tree is complete. The driver receives the root.
- **The semantic analyzer walks the AST** once, depth-first. It opens and closes scopes in the symbol table as it enters and leaves blocks, writes a type into every expression node, and writes a unique TAC name into every variable reference.
- **The TAC generator walks the annotated AST** and only runs if no lexical, syntax or semantic error was found. It relies on two things the earlier phases guarantee: the tree's shape already reflects operator precedence, and every name has been resolved to a unique variable.
- **The driver** (`src/main.c` or `src/main.cpp`) runs the phases in order and stops after the first phase that reports errors. Options print the output of a single phase: `--tokens`, `--ast`, `--dot`, `--symtab`, `--tac` (the default), and the bonus `--opt`, `--run`, `--asm`, or `--all` for every phase. The exit status is 0 for a correct program and 1 for a program with errors.
- **The web interface** (`server/`, `frontend/`) is a FastAPI server that runs a compiler once per phase and a React editor that shows each phase in its own tab, draws the AST as a tree, and underlines errors on their line.

**[Screenshot]** the web interface with the TAC tab open on the manual's sample program.

## 5. Lexer Design

All three lexers are Flex specifications built on the same principles.

### 5.1 Token definitions and regular expressions

| Token | Regular expression / text | Notes |
|---|---|---|
| keywords | `int float bool if else while print true false` | listed **before** the identifier rule |
| `IDENT` | `[A-Za-z_][A-Za-z0-9_]*` | the name is copied into `yylval` |
| `INT_LITERAL` | `[0-9]+` | |
| `FLOAT_LITERAL` | `[0-9]+"."[0-9]+` (C, Java also allows an exponent; C++ also `.5` and `5.`) | |
| multi-character operators | `== != <= >= && \|\|` | longest match beats `=`, `<`, `&` ... |
| single-character tokens | `+ - * / % < > = ! ( ) { } ;` | returned as their character code |
| comments | `"//"[^\n]*` and `"/*"` ... `"*/"` | discarded; block comments use an exclusive start condition |
| whitespace | `[ \t\r\n]+` | discarded |
| malformed identifier | `[0-9]+{ID}` such as `2count` | reported, then returned as an identifier |
| anything else | `.` | reported as an invalid character, then skipped |

### 5.2 Design decisions

- **Keywords before identifiers.** When two rules match the same longest text, Flex chooses the rule listed first. Listing `while` before the identifier pattern makes `while` a keyword, while `whilex` is still an identifier because it is a longer match for the identifier rule (manual tip, section 4.1).
- **Line numbers.** `%option yylineno` tracks the current line, and `YY_USER_ACTION` copies it into `yylloc` for every token, so every later phase knows where each construct came from.
- **Errors do not stop scanning.** An invalid character is reported (`line 2: lexical error: invalid character '@'`) and skipped, so every bad character in a file is reported in one run. A malformed identifier such as `2count` is reported and then passed on as an identifier, so the parser does not add a second, misleading syntax error.
- **Unterminated block comments.** `/*` enters an exclusive `COMMENT` start condition that consumes everything up to `*/`. Reaching end-of-file inside it reports `unterminated block comment` at the line where the comment opened.
- **Previous-token tracking (C and Java).** A small wrapper around the generated scanner remembers the previous token and its line, which the parser uses to put "missing `;`" errors on the right line (chapter 6.4).
- **Real-language tokens.** Each lexer also recognises the extra tokens of its language style: `#include <header>` lines (C, C++), `import` lines (Java), string literals for `printf` and `cout`, and `System.out.println` as a single token in Java.

Output of `--tokens` (C compiler) for the manual's section 4.6 example:

```text
Line   Token            Lexeme
1      KEYWORD_INT      int
1      IDENTIFIER       a
1      DELIMITER        ;
2      KEYWORD_INT      int
...
```

## 6. Parser Design

### 6.1 Grammar in Bison

Each `parser.y` implements the grammar of chapter 3.2. Statements follow the BNF directly. Expressions use one compact, ambiguous rule

```text
expression : expression '+' expression | expression '*' expression | ... | '!' expression | '-' expression %prec UMINUS
           | '(' expression ')' | INT_LITERAL | FLOAT_LITERAL | TRUE | FALSE | IDENT
```

made unambiguous by precedence and associativity declarations.

### 6.2 Precedence and associativity

From lowest to highest, the same in all three compilers:

```text
%left OR                 ||
%left AND                &&
%left EQ NE              == !=
%left '<' '>' LE GE      < > <= >=
%left '+' '-'            + -
%left '*' '/' '%'        * / %
%right '!' UMINUS        unary ! and -
```

- **Precedence** decides between shifting and reducing when two operators meet. In `a + b * 2`, `*` has the higher precedence, so `b * 2` is reduced first and becomes the right child of `+`.
- **Left associativity** makes `a - b - c` mean `(a - b) - c`.
- **Unary minus** uses `%prec UMINUS`, because the `-` token is also binary minus and needs a different precedence when it is unary.

### 6.3 Conflict resolution: the dangling else

In `if (a) if (b) S1 else S2` the parser cannot tell, at `else`, whether to attach it to the inner or the outer `if`. This is the classic shift/reduce conflict. It is resolved by giving the `ELSE` token a higher precedence than the rule for an `if` without `else` (`%prec IF_WITHOUT_ELSE` in C, `%prec LOWER_THAN_ELSE` in C++ and Java). The parser then shifts the `else`, attaching it to the nearest `if`, as in C, C++ and Java.

With these declarations Bison reports **no shift/reduce and no reduce/reduce conflicts** for any of the three grammars, including all the bonus constructs.

### 6.4 Error reporting and recovery

- `%error-verbose` makes Bison describe the problem, for example `unexpected '{'` or `unexpected IDENT, expecting ';'`. Every message carries the line number of the offending token.
- **Recovery** uses Bison's `error` token. The rules `statement : error ';' | error '}'` let the parser discard the broken part of a statement, resynchronise at the next `;` or `}`, and continue, so independent syntax errors in one file are all reported. The C compiler also has a rule for a `do-while` that is missing its final `;`.
- **Missing semicolons on the right line.** Bison only notices a missing `;` at the next token, which is usually on the next line. When the offending token starts a new line and the previous line did not end in `;`, `{` or `}`, the C and Java compilers report the previous line with a hint:

```text
line 3: syntax error: unexpected IDENT, expecting ';' or '=' on line 4 (missing ';' at the end of line 3?)
```

The grammars only use features available since Bison 2.4 (`%error-verbose`, empty rules written as comments, `%nonassoc`), because that is the version on the lab machines.

## 7. Abstract Syntax Tree

### 7.1 Node structure

The AST keeps only meaningful constructs: parentheses, semicolons and the `class`/`main` wrapper only shape the tree and never become nodes. Every node stores its source line, and every expression node has a type field that the semantic analyzer fills in.

| Compiler | Representation |
|---|---|
| C | two C structs, `Expr` and `Stmt`, each with a `kind` tag and a union of per-kind fields |
| C++ | a class hierarchy: `Node` with subclasses such as `IfNode`, `WhileNode`, `BinaryNode`, `AssignNode`, `VarDeclNode`, each with a virtual `print` |
| Java | one generic `Node` with a `kind` string, a `text`, a line and a list of children |

The node kinds of the core language are the same in all three:

| Expression nodes | Statement nodes |
|---|---|
| `IntLiteral`, `FloatLiteral`, `BoolLiteral` | `VarDecl` (type, name, optional initializer) |
| `Identifier` | `Assign` (name, value) |
| `BinaryOp` (operator, left, right) | `Print` (value) |
| `UnaryOp` (operator, operand) | `If` / `IfElse` (condition, then, else) |
| | `While` (condition, body) |
| | `Block` (list of statements: a new scope) |

The bonus constructs add `For`, `DoWhile`, `Switch`, `Case`, `Break`, `Continue`, `ArrayDecl`, `ArrayIndex`, `Function`, `Call` and `Return` nodes.

### 7.2 Construction strategy

Nodes are created in the Bison actions as each rule is reduced, for example `$$ = ast_binary(OP_ADD, $1, $3, line);`. Because the parser reduces the operands of an operator before the operator itself, the tree is built bottom-up and its shape already encodes precedence and associativity. Shorthand forms are rewritten into core nodes where that keeps later phases simple: in C++, `x += e` becomes `x = x + e` and `for` becomes a `while` node that carries the update step.

### 7.3 Printing and visualization

`--ast` prints the tree with two spaces of indentation per level. For `c = a + b * 2;` (C compiler):

```text
Assign c (line 6)
  BinaryOp +
    Identifier a
    BinaryOp *
      Identifier b
      IntLiteral 2
```

`--dot` (bonus) prints the same tree as a Graphviz graph, with nodes coloured by kind, which `dot -Tpng ast.dot -o ast.png` turns into a picture:

```text
digraph AST {
    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=11];
    n0 [label="Program", fillcolor="#d9d2e9"];
    n1 [label="VarDecl int a (line 1)", fillcolor="#cfe2f3"];
    n0 -> n1;
    ...
}
```

The web interface also draws the AST as a foldable, zoomable diagram; clicking a node jumps to its line in the editor.

**[Screenshot]** the AST diagram of the manual's sample program in the web interface.

## 8. Semantic Analysis

### 8.1 Rules and how each is detected

The semantic analyzer walks the AST depth-first, opening a scope in the symbol table for every block and closing it at the end.

| Rule (manual 4.5) | How it is detected | Example message (C compiler) |
|---|---|---|
| Undeclared variable | the lookup finds no visible symbol, and no symbol with that name was ever declared | `line 3: semantic error: undeclared variable 'undeclaredVar'` |
| Redeclaration | the name already exists in the **current** scope | `line 2: semantic error: redeclaration of 'x' (already declared at line 1 in this scope)` |
| Scope violation | the name is not visible now, but it was declared in a block that has already closed | `line 8: semantic error: scope violation: 'inner' (declared at line 5) is not visible outside its block` |
| Type mismatch | an initializer's type cannot be stored in the declared type; or a condition is not bool | `line 9: semantic error: type mismatch: cannot initialize bool variable 'b' with a float value` |
| Invalid assignment | the assigned value's type cannot be stored in the variable | `line 11: semantic error: invalid assignment: cannot assign a bool value to int variable 'i'` |
| Invalid expression | the operand types break the typing table of chapter 3.3 | `line 12: semantic error: invalid expression: logical operator '&&' needs bool operands, got int and int` |

All three compilers report all six, each with a line number and a description. The C++ compiler uses the format `Semantic error [line 9]: ...`, and the Java compiler says `boolean` instead of `bool`.

### 8.2 Avoiding cascades of errors

An expression whose type cannot be determined gets an internal `error` type, and every check involving an `error` operand stays silent. One mistake, such as an undeclared variable inside a long expression, therefore produces exactly one message. Analysis continues after an error, so every semantic error in the file is reported in one run.

### 8.3 Order of checks in a declaration

In `int x = x;` the initializer must not see the new `x`. The C and C++ analyzers check the initializer **before** inserting the name, so the right-hand `x` is reported as undeclared. The Java analyzer follows Java's rule instead: the name is declared first but is not yet *definitely initialized*, so it reports `variable 'x' might not have been initialized`.

### 8.4 Additional rules

- **All three:** `if` and `while` conditions must be bool; `break` only inside a loop or `switch`, `continue` only inside a loop; `switch` on an int, without duplicate `case` values or two `default`s.
- **Functions:** a call must name a function, with one argument of a fitting type per parameter; a `void` function cannot be used as a value; `return` must match the function's type; `main` cannot be called.
- **Arrays:** a positive size, at most that many initial values, an `int` index, and an array is only used with an index.
- **Java only:** definite initialization. Before an `if`, the analyzer copies the symbol table, checks each branch on its own copy and merges them: a variable counts as initialized afterwards only if both branches initialize it. A non-void method that can end without `return` is reported as a "missing return statement".
- **Real-language forms:** `printf` conversions must match the types of their arguments (C); `cout`/`cin` need `#include <iostream>` (C++); `Scanner` needs `import java.util.Scanner` (Java).

## 9. Symbol Table

### 9.1 Data structure

Each compiler implements the symbol table as a **stack of scopes**. The global scope is at the bottom; entering a block pushes a new scope and leaving it pops the scope.

| Compiler | Representation of a scope |
|---|---|
| C | a struct with a unique id, a depth, a linked list of symbols and a pointer to the enclosing scope |
| C++ | a `std::map<std::string, Symbol>` in a `std::vector` used as a stack |
| Java | a `std::unordered_map<std::string, SymbolInfo>` in a `std::vector` used as a stack |

Each entry records at least the fields the manual asks for, plus what later phases need:

| Field | Purpose |
|---|---|
| name | the identifier |
| type | `int`, `float` or `bool` (and, for the bonus, an array length or a function's parameter types) |
| scope | the unique id of the declaring block, and its nesting depth |
| line declared | used in error messages such as "already declared at line 1" |
| TAC name | the unique name used for this variable in the TAC |
| initialized (Java) | whether the variable is definitely assigned |

### 9.2 Operations and scope handling

- **Insert** adds a symbol to the current scope and fails if that scope already contains the name: this is how a redeclaration is detected. A name declared in an outer scope may be declared again in an inner one (shadowing).
- **Lookup** searches from the innermost scope outwards, so an inner declaration hides an outer one.
- **Enter / exit scope** push and pop a scope. Once a scope is popped its symbols can no longer be found, which is exactly what makes a variable declared inside an `if` or `while` block invisible after the block (manual section 4.4).
- **History.** Every symbol ever declared is also kept, in declaration order, in a history list. `--symtab` prints this list, and it is how a **scope violation** (declared in a closed block) is told apart from an **undeclared** name (never declared).

### 9.3 Unique TAC names

In the TAC all variables share one flat namespace. If an inner block declares another `n`, the two must not share storage. Every later declaration of an existing name therefore gets a unique TAC name: `n@3` in C and Java (`@` and the scope id) and `n_1` in C++. These characters cannot appear in a source identifier, so the new names never clash.

`--symtab` (C compiler) for a program with a function, an array and a `for` loop:

```text
Name             Type     Scope  Depth  Line   TAC name
----             ----     -----  -----  ----   --------
fact             int(int) 0      0      1      fact
n                int      1      1      1      n
a                int[3]   0      0      7      a
sum              int      0      0      8      sum
i                int      3      1      9      i
```

## 10. Intermediate Code

### 10.1 Instruction set

The TAC generator walks the checked AST and writes one instruction per line, with at most one operator per instruction:

| Form | Meaning |
|---|---|
| `x = y` | copy |
| `t1 = a op b` | binary operation (arithmetic, relational or logical) |
| `t1 = op a` | unary operation (`-` or `!`) |
| `ifFalse c goto L1` | jump to `L1` if `c` is false |
| `if c goto L1` | jump to `L1` if `c` is true |
| `goto L1` | unconditional jump |
| `L1:` | label |
| `print x` | output |

The C++ compiler's TAC is typed: when an int meets a float it inserts an explicit conversion, `t2 = (float) a`.

### 10.2 Generation strategy

**Expressions** are generated in post-order: the code for both operands first, then the operator, whose result goes into a new temporary `t1, t2, ...`. The generator returns the name holding the value (a variable, a constant or a temporary) to its caller. Because the AST already reflects precedence, this simple walk evaluates operators in the right order. The manual's section 4.6 example gives exactly the manual's output in all three compilers:

```text
int a; int b; int c;          a = 5
a = 5;                        b = 10
b = 10;              ==>      t1 = b * 2
c = a + b * 2;                t2 = a + t1
                              c = t2
```

**Logical operators.** The C and C++ compilers evaluate `&&` and `||` as ordinary binary operators. The Java compiler short-circuits them, as Java does: the right operand is only evaluated when it can still change the result, using `ifFalse`/`if` jumps.

**Control flow** uses labels and jumps:

```text
if (c) S1 else S2            while (c) S
    <code for c>             L1:
    ifFalse c goto L1            <code for c>
    <S1>                         ifFalse c goto L2
    goto L2                      <S>
L1:                              goto L1
    <S2>                     L2:
L2:
```

An `if` without `else` needs only the first label, and a declaration without an initializer produces no code.

### 10.3 Complete example: the manual's sample program (section 5.5)

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

All three compilers produce this TAC; run with `--run`, it prints `55`.

### 10.4 Bonus constructs in the TAC

`for`, `do-while` and `switch` are translated into the same labels and jumps (`break` and `continue` become `goto`s to the loop's or switch's labels). An array element is read with `t = a[i]` and written with `a[i] = t`, where the index counts elements. A function is a `func f:` ... `endfunc` block whose parameters are taken with `formal x`; a call passes each argument with `param` and then calls it:

```text
int fact(int n) {                 func fact:
    if (n <= 1) {                 formal n
        return 1;                 t1 = n <= 1
    }                             ifFalse t1 goto L1
    return n * fact(n - 1);       return 1
}                                 L1:
                                  t2 = n - 1
                                  param t2
                                  t3 = call fact 1
                                  t4 = n * t3
                                  return t4
                                  endfunc
```

### 10.5 Beyond the required output

TAC is the final required output (manual section 4.6). The optional back end works on the generated TAC:

- **Optimizer (`--opt`, section 14):** constant propagation and folding inside each basic block, removal of branches whose condition is known, removal of unreachable code after a `goto` or `return`, removal of unused temporaries, and removal of jumps to the next instruction and of unused labels, repeated until nothing changes. For example, 14 TAC instructions for a small constant program become:

  ```text
  a = 5
  b = 13
  c = 8
  big = true
  print 8
  ```

- **Interpreter (`--run`)** executes the TAC, reading input from standard input. A call saves the called function's own variables and restores them on return, so recursion works even though every variable has a single cell. Division by zero, an array index out of bounds and endless loops stop the program with a runtime error.
- **Assembly (`--asm`)** translates the optimized TAC into 32-bit x86 (AT&T syntax) for MinGW `gcc`. The output of the assembled programs was checked against `--run`.

## 11. Challenges

| Challenge | How it was resolved |
|---|---|
| The lab machines have Bison 2.4 and GNU Make 3.81, which reject newer syntax such as `%define parse.error`, `%empty` and `%precedence`. | The grammars and Makefiles only use features available since Bison 2.4 and Make 3.81, so `make` works on both old and new toolchains. |
| The dangling else caused a shift/reduce conflict. | `ELSE` was given a higher precedence than the `if` rule without `else`, so the parser shifts it (chapter 6.3). |
| Bison notices a missing `;` only at the next token, so the error pointed at the wrong line. | The lexer remembers the previous token and its line; when the offending token starts a new line, the previous line is reported with a "missing `;`" hint. |
| One bad operand caused several follow-on type errors. | An internal `error` type is propagated and every check on it stays silent (chapter 8.2). |
| A malformed identifier such as `2count` produced a lexical error and a confusing syntax error. | The lexer reports it and then returns it as an identifier, so parsing continues normally. |
| A shadowing variable overwrote the outer variable in the TAC. | Unique TAC names (`n@3`, `n_1`) chosen by the symbol table (chapter 9.3). |
| Telling "never declared" apart from "declared in a block that has closed". | Every declaration is kept in a history list that is searched when a name is not visible. |
| Java's "might not have been initialized" rule across `if` branches and loops. | Copy the symbol table, check each path separately, then merge: initialized only if every path initializes it. |
| The manual's sample program is not valid C, C++ or Java. | Each compiler accepts both forms: a file without `main` (or without a class) is compiled as the manual's plain list of statements, with no flags. |
| Recursion, when every variable lives in one memory cell. | A call saves the called function's locals (variables, arrays and temporaries) and restores them on return, both in the interpreter and in the generated assembly. |
| The optimizer removed function calls whose result was unused, and treated code after `endfunc` as unreachable. | Calls are always kept (they may print), what is known about variables is forgotten after a call and at function boundaries, and `func` / `endfunc` end an unreachable region. |
| In Java, accepting both `public class` and bare `public static` methods created a grammar conflict. | Methods written among the statements of the bare form start with `static`, which keeps the grammar conflict-free. |
| Test output differed between Windows (CRLF) and Linux (LF). | The test scripts strip `\r` before comparing. |

## 12. Testing

### 12.1 Test organisation

Each compiler has a `tests/` folder with the four categories required by section 15. Each test program is paired with a `.expected` file holding its exact expected output (and, for C and Java, its exit status). A test can set compiler options on its first line (`// flags: --run`) and give program input. `make test` compiles every test program and compares the actual output with the expected one.

| Category | C | C++ | Java | What the tests demonstrate |
|---|---|---|---|---|
| `valid/` | 23 | 25 | 22 | non-trivial programs compiling cleanly to TAC: the manual's sample (5.5) and TAC example (4.6), nested scopes and shadowing, every operator, input, loops and `switch`, arrays and recursive functions, optimization, `--run`, `--asm` and `--dot` |
| `lexical/` | 5 | 7 | 5 | invalid characters, identifiers starting with a digit, unterminated comments and strings, unknown headers or imports |
| `syntax/` | 4 | 9 | 5 | missing semicolons and parentheses, unbalanced braces, broken `for` / `do-while`, a misplaced or missing `main`, several errors in one file |
| `semantic/` | 14 | 10 | 11 | one program for each of the six rules of section 4.5, plus `printf`/`scanf`, `Scanner`, initialization, `break`/`continue`/`switch`, array and function rules |
| **Total** | **46** | **51** | **43** | |

### 12.2 Results

```sh
make -C backend/c-compiler test        # 46 passed, 0 failed
make -C backend/c++_compiler test      # Passed: 51  Failed: 0
make -C backend/javacompiler test      # 43 passed, 0 failed
```

All 140 tests pass. The Docker build runs the three suites, so a deployment fails if any test fails.

### 12.3 Required test categories (section 15)

| Section 15 requirement | Example test and result |
|---|---|
| A non-trivial valid program compiling to TAC | `valid/manual_sample`: the TAC of chapter 10.3 |
| An invalid token | `lexical/invalid_character`: `line 2: lexical error: invalid character '@'` |
| A grammar violation | `syntax/missing_semicolon`: `syntax error: unexpected PRINT on line 2 (missing ';' at the end of line 1?)` |
| Undeclared variable | `semantic/undeclared` |
| Redeclaration | `semantic/redeclaration` |
| Scope violation | `semantic/scope_violation` |
| Type mismatch | `semantic/type_mismatch`: the manual's own `bool b = 5 + 3.2;` |
| Invalid assignment | `semantic/invalid_assignment` |
| Invalid expression | `semantic/invalid_expression`: `&&` on numbers |

The same probe programs were also run through all three compilers side by side to confirm that they agree on the manual's sample, its TAC example, every operator, comments, nested scopes, and the lexical, syntax and semantic errors.

**[Screenshot]** `make` building each compiler.
**[Screenshot]** `make test` with every test passing.
**[Screenshot]** `--all` on the manual's sample program.
**[Screenshot]** a lexical, a syntax and a semantic error.

## 13. Conclusion

Building three compilers for one language made the structure of a compiler front end very concrete. Each phase depends on the one before it: the line numbers the lexer records reach the semantic error messages; the precedence declared in the parser is what makes a simple post-order walk produce correct TAC; and the scope stack in the symbol table decides both which errors are reported and how variables are named in the TAC.

We learned that rejecting invalid programs well takes as much care as compiling valid ones: reporting the right line, the right category and the real cause, and not burying one mistake under a pile of follow-on errors. Error recovery in the parser and the `error` type in the semantic analyzer were small changes with a large effect on the quality of the messages.

Comparing the three implementations also showed which parts of a compiler depend on the source language and which do not. Supporting C, C++ or Java syntax changed only the lexer, the top of the grammar and a few semantic rules; the AST, symbol table and TAC generator work the same way in all three. Adding arrays, functions and recursion showed how a runtime convention (saving a function's locals on a call) has to be respected consistently by the TAC generator, the optimizer, the interpreter and the assembly generator.

## 14. References

1. *Compiler Construction Lab: Project Manual*, Department of CSE, Metropolitan University, Bangladesh.
2. A. V. Aho, M. S. Lam, R. Sethi and J. D. Ullman, *Compilers: Principles, Techniques, and Tools*, 2nd ed., Addison-Wesley, 2006 (chapters 2-6: lexical analysis, syntax analysis, syntax-directed translation, intermediate code).
3. *GNU Bison manual*, <https://www.gnu.org/software/bison/manual/>.
4. *Flex: The Fast Lexical Analyzer manual*, <https://westes.github.io/flex/manual/>.
5. J. Gosling et al., *The Java Language Specification*, chapter 16 (Definite Assignment).
6. *Graphviz DOT language*, <https://graphviz.org/doc/info/lang.html>.
7. AI assistance (Claude) was used while writing and restructuring code and documentation, as permitted by section 10 of the manual. Every member has reviewed the work and must be able to explain every part of it.
