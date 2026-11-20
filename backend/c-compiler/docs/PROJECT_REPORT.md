# Tricora C Compiler: Project Report

Compiler Construction Lab, Metropolitan University, Department of CSE
Component: compiler front-end in C (Flex + Bison), `backend/c-compiler`

This report follows the chapter structure in section 12 of the lab manual. Screenshots of builds, runs and error handling (manual section 11) go in the places marked **[Screenshot]**.

---

## 1. Introduction

A compiler front-end turns source text into a form a later stage can work with. Throughout the semester the phases (lexing, parsing, semantic analysis, intermediate code) were studied one at a time. This project connects them into one pipeline for the fixed mini language of the lab manual. Each phase's output becomes the next phase's input, and an error in any phase is reported clearly before compilation stops.

## 2. Objectives

- Recognise every token of the language with a Flex lexer, and report invalid tokens with line numbers.
- Check program structure with an unambiguous Bison grammar, report syntax errors, and recover from them.
- Build an Abstract Syntax Tree and print it in readable form.
- Keep a symbol table with nested scopes.
- Enforce the semantic rules of manual section 4.5.
- Generate Three Address Code for expressions, control flow and `print`.
- Back each of the above with test programs and their expected output.

## 3. Language specification

**Types:** `int` (signed integer), `float` (floating point), `bool` (`true`/`false`).

**Statements:** declaration (`int x;` or `int x = e;`), assignment (`x = e;`), `print e;`, `if`, `if-else`, `while`, and nested blocks `{ ... }`, each of which opens a new scope.

**Operators:** arithmetic `+ - * / %`, relational `< > <= >= == !=`, logical `&& || !`, unary minus.

**Lexical elements:** identifiers `[A-Za-z_][A-Za-z0-9_]*`; integer literals such as `42`; float literals such as `3.14`; `//` and `/* */` comments.

The full formal CFG, the precedence table and the lexical rules are in [`grammar.md`](grammar.md). The core of the grammar:

```text
<program>    ::= <stmt-list>
<stmt-list>  ::= <stmt-list> <statement> | ε
<statement>  ::= <type> IDENT ";" | <type> IDENT "=" <expr> ";" | IDENT "=" <expr> ";"
               | "print" <expr> ";" | "if" "(" <expr> ")" <statement>
               | "if" "(" <expr> ")" <statement> "else" <statement>
               | "while" "(" <expr> ")" <statement> | <block>
<block>      ::= "{" <stmt-list> "}"
<type>       ::= "int" | "float" | "bool"
```

**Typing rules:**

| Construct | Allowed operands | Result |
|---|---|---|
| `+ - * /` | int/float | int if both are int, otherwise float |
| `%` | int, int | int |
| `< > <= >=` | int/float | bool |
| `== !=` | two numbers, or two bools | bool |
| `&& \|\|`, `!` | bool | bool |
| unary `-` | int/float | same type |
| assignment / initializer | value type equals variable type, or int stored into float | |
| `if` / `while` condition | bool | |

## 4. Compiler architecture

```text
 source file (.tri)
       |
       v
 +----------------------+   tokens (+ line numbers)
 | Lexical analyzer     |---------------------------+
 | src/lexer/lexer.l    |                           |
 +----------------------+                           v
                                  +-------------------------------+
                                  | Syntax analyzer               |
                                  | src/parser/parser.y           |
                                  | builds AST (src/ast/)         |
                                  +-------------------------------+
                                                    | AST
                                                    v
                                  +-------------------------------+
                                  | Semantic analyzer             |
                                  | src/semantic/ + symbol table  |
                                  | src/symbol_table/             |
                                  +-------------------------------+
                                                    | annotated AST
                                                    v
                                  +-------------------------------+
                                  | TAC generator                 |
                                  | src/codegen/                  |
                                  +-------------------------------+
                                                    |
                                                    v
                                         Three Address Code
```

The driver (`src/main.c`) runs the phases in order. Bison calls the lexer whenever it needs the next token (`yylex`), so lexing and parsing run together. The parser stores the finished tree in `program_root`. The semantic analyzer walks this tree, fills the symbol table and writes a type into every expression node. TAC is generated only if no lexical, syntax or semantic error was found.

The options `--tokens`, `--ast`, `--symtab`, `--tac` and `--all` show the output of each phase separately.

## 5. Lexer design

The Flex specification lists the patterns in this order:

1. **Comments and whitespace.** `//[^\n]*` is a single-line comment. `/*` switches to an exclusive start condition `COMMENT`, which consumes everything up to `*/`. If end-of-file is reached inside `COMMENT`, an *unterminated block comment* error is reported with the line where the comment began. Whitespace `[ \t\r\n]+` is discarded.
2. **Keywords** are listed before the identifier rule. When two rules match the same longest text, Flex picks the earlier rule, so `while` becomes `WHILE` and not `IDENT`.
3. **Multi-character operators** (`== != <= >= && ||`). Longest match makes `<=` win over `<`.
4. **Literals:** `[0-9]+\.[0-9]+` gives `FLOAT_LITERAL` and `[0-9]+` gives `INT_LITERAL`. The value is passed to the parser in `yylval`.
5. **Malformed identifier** `[0-9]+{ID}` (for example `2count`) is reported as a lexical error and then returned as an identifier, so the parser does not add a second, misleading error.
6. **Identifiers** `[A-Za-z_][A-Za-z0-9_]*`. A copy of the name goes to the parser.
7. **Single-character tokens** `- + * / % < > = ! ( ) { } ;` are returned as their character code.
8. **Anything else** (`.`) is an *invalid character* lexical error. Scanning continues, so every bad character is reported.

`%option yylineno` keeps the current line number. `YY_USER_ACTION` copies it into `yylloc` for every token, which gives the parser line numbers for its error messages and AST nodes. A small wrapper around the generated scanner also remembers the previous token and its line. The parser uses this to place "missing `;`" errors on the right line (see chapter 11).

## 6. Parser design

The grammar in `parser.y` follows chapter 3. Expressions use one ambiguous rule (`expression OP expression`), made unambiguous by Bison's precedence declarations (lowest first):

```text
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right '!' UMINUS
%nonassoc IF_WITHOUT_ELSE
%nonassoc ELSE
```

- **Precedence** decides between shifting and reducing when two operators meet: in `a + b * 2`, `*` is higher, so `b * 2` is reduced first.
- **Left associativity** makes `a - b - c` mean `(a - b) - c`.
- **Unary minus** uses `%prec UMINUS` because the `-` token is also binary minus.
- **Dangling else:** `ELSE` has higher precedence than the `if` rule without else (`%prec IF_WITHOUT_ELSE`), so the parser shifts `else` and attaches it to the nearest `if`.

With these declarations Bison reports **0 shift/reduce and 0 reduce/reduce conflicts**.

**Error reporting and recovery.** `%error-verbose` makes Bison say what it found (and, where there are only a few options, what it expected). `yyerror` prints `line N: syntax error: ...` using the line of the offending token. Two extra rules, `statement: error ';' | error '}'`, let the parser throw away the broken part of a statement, resynchronise at the next `;` or `}`, and keep parsing. The result is that `tests/syntax/multiple_errors.tri` reports both of its errors in one run.

## 7. Abstract Syntax Tree

`src/ast/ast.h` defines two node families, each a C struct with a `kind` tag and a union of per-kind fields:

| Expression nodes (`Expr`) | Statement nodes (`Stmt`) |
|---|---|
| `IntLiteral`, `FloatLiteral`, `BoolLiteral` | `VarDecl` (type, name, optional initializer) |
| `Identifier` | `Assign` (name, value) |
| `BinaryOp` (operator, left, right) | `Print` (value) |
| `UnaryOp` (operator, operand) | `If` / `IfElse` (condition, then, else) |
| | `While` (condition, body) |
| | `Block` (list of statements) |

Every node stores its source line. Expression nodes also have a `type` field, which the semantic analyzer fills in. Nodes are built bottom-up in the Bison actions (for example `$$ = ast_binary(OP_ADD, $1, $3, line)`), so when the start rule is reduced, the whole program tree is complete. Parentheses and semicolons only shape the tree; they never become nodes.

`--ast` prints the tree with indentation. For `c = a + b * 2;`:

```text
Assign c (line 7)
  BinaryOp +
    Identifier a
    BinaryOp *
      Identifier b
      IntLiteral 2
```

## 8. Semantic analysis

`src/semantic/semantic.c` walks the AST depth-first. It opens a scope for each `Block` and closes it at the end. Each rule is detected as follows:

| Rule (manual 4.5) | How it is detected | Example message |
|---|---|---|
| Undeclared variable | `symtab_lookup` finds no visible symbol and no symbol of that name was ever declared | `line 2: semantic error: undeclared variable 'count'` |
| Redeclaration | `symtab_lookup_current` finds the name already in the *current* scope | `redeclaration of 'value' (already declared at line 1 in this scope)` |
| Scope violation | not visible now, but declared earlier in a block that has been closed | `scope violation: 'local' (declared at line 2) is not visible outside its block` |
| Type mismatch | initializer type is not assignable to the declared type | `type mismatch: cannot initialize bool variable 'b' with a float value` |
| Invalid assignment | assigned value's type is not assignable to the variable | `invalid assignment: cannot assign a bool value to int variable 'count'` |
| Invalid expression | operand types break the typing table in chapter 3 | `invalid expression: logical operator '&&' needs bool operands, got bool and int` |
| Non-bool condition | `if`/`while` condition is not bool | `type mismatch: while condition must be bool, got int` |

An expression whose type cannot be determined gets the internal type `error`, and no further messages are produced for it. That way one mistake does not cause a chain of follow-on errors. Analysis continues after an error, so every semantic error in the file is reported. In a declaration, the initializer is checked *before* the new name is inserted, so `int x = x;` correctly reports `x` as undeclared.

## 9. Symbol table

`src/symbol_table/` implements a **stack of scopes**. Each scope has a unique id, a nesting depth, a list of its symbols and a pointer to the enclosing scope. The global scope has id 0.

| Field | Purpose |
|---|---|
| `name` | identifier |
| `type` | int, float or bool |
| `scope_id`, `depth` | which block declared it, and how deeply that block is nested |
| `line` | declaration line, used in error messages |
| `tac_name` | name used in the TAC (see chapter 10) |

- **Insert** adds the symbol to the current scope, and fails if the name already exists in that same scope (a redeclaration).
- **Lookup** searches from the current scope outwards through the parent pointers. An inner declaration therefore hides (shadows) an outer one.
- **Enter / exit scope** push and pop a scope. A popped scope's symbols can no longer be found, which is exactly what makes a block-local variable invisible outside its block.
- All symbols are also kept in declaration order in a history list. `--symtab` prints that list, and it is how a scope violation is told apart from a plain undeclared name.

`--symtab` for the manual's sample program:

```text
Name             Type   Scope  Depth  Line   TAC name
----             ----   -----  -----  ----   --------
x                int    0      0      2      x
y                int    0      0      3      y
flag             bool   0      0      4      flag
```

## 10. Intermediate code

`src/codegen/codegen.c` walks the checked AST and prints TAC. Each instruction has at most one operator:

| Form | Meaning |
|---|---|
| `x = y` | copy |
| `t1 = a op b` | binary operation |
| `t1 = op a` | unary operation (`-` or `!`) |
| `ifFalse c goto L1` | conditional jump |
| `goto L1` | unconditional jump |
| `L1:` | label |
| `print x` | output |

**Expressions** are generated in post-order: both operands first, then the operator, whose result goes into a new temporary `t1, t2, ...`. Because the AST already reflects precedence, the order is correct.

```text
c = a + b * 2;   ==>   t1 = b * 2
                       t2 = a + t1
                       c = t2
```

**Control flow:**

```text
if (c) S1 else S2          while (c) S
    <code for c>           L1:
    ifFalse c goto L1          <code for c>
    <S1>                       ifFalse c goto L2
    goto L2                    <S>
L1:                            goto L1
    <S2>                   L2:
L2:
```

An `if` without `else` needs only the first label. Declarations without an initializer produce no code.

**Shadowing.** In TAC all variables live in one flat namespace. If an inner block declares another `n`, the two variables must not share a name. The symbol table therefore gives every later declaration of an existing name a unique TAC name `name@scope` (for example `n@3`). `@` cannot appear in a source identifier, so the new name can never clash.

Full TAC of the manual's sample program (section 5.5):

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

### Back end (beyond the manual's required phases)

The manual's required output is the TAC (section 4.6); everything below is extra. `src/backend/` reads the TAC text written by the generator back into a list of instructions. TAC carries no types, so the driver passes the declared type of every variable from the symbol table, and the type of each temporary is worked out from the instruction that defines it. On that list:

- **Optimizer (`--opt`, a manual section 14 bonus):** constant propagation and folding inside each basic block (known values are forgotten at every label), conditional jumps with a known condition become `goto` or disappear, code after an unconditional `goto` is removed up to the next label, unused temporaries are dropped, `t = a op b; x = t` becomes `x = a op b`, and `goto L` directly before `L:` and labels nobody jumps to are removed. The passes repeat until nothing changes.
- **Interpreter (`--run`):** executes the optimized TAC. int arithmetic is 32-bit, `print` writes ints, floats (`%g`) and `true`/`false`; division or modulo by zero stops with `Runtime error: ...` (exit status 1), and an endless loop is stopped after 10 million steps.
- **Assembly (`--asm`):** 32-bit x86 in AT&T syntax for MinGW `gcc`, in the style of the C++ compiler: every variable and temporary has a memory cell, ints and bools go through `%eax`/`%ecx`, floats through the x87 stack, and `print` calls `printf`.

The tests `asm_small`, `opt_constant_folding`, `run_*` cover these options; the interpreter's output was also checked against the assembled program run natively.

## 11. Challenges

| Challenge | Resolution |
|---|---|
| The lab machines have Bison 2.4 and GNU Make 3.81, which reject newer syntax (`%define parse.error`, `%empty`, `%precedence`, grouped `&:` make targets). | The grammar and Makefile only use features available since Bison 2.4 / Make 3.81 (`%error-verbose`, empty rules, `%nonassoc`/`%right`), so `make` works on old and new toolchains. |
| Dangling else caused a shift/reduce conflict. | `ELSE` was given higher precedence than the `if` rule without else. |
| Bison only notices a missing `;` at the *next* token, which is usually on the next line, so the error pointed at the wrong line. | The lexer remembers the previous token and its line. When the bad token starts a new line and the previous line did not end in `;`, `{` or `}`, the error is reported on the previous line with a "missing `;`" hint. |
| A malformed identifier such as `2count` produced a lexical error plus a confusing syntax error. | The lexer reports it and then returns it as an identifier, so parsing continues normally. |
| One bad operand caused several follow-on type errors. | An internal `error` type is propagated, and checks on it are silenced. |
| A shadowing variable overwrote the outer variable in the TAC. | Unique TAC names (`n@3`) are assigned by the symbol table. |
| Telling "never declared" apart from "declared in a block that has closed". | Every declaration is kept in a history list that is searched when a name is not visible. |

## 12. Testing

`make test` runs `tests/run_tests.sh`. It compiles every `tests/<category>/*.tri` program and compares the output and exit status with `<name>.expected`. A test can pass compiler options on its first line (`// flags: --run`). Current result: **24 passed, 0 failed**.

| Category | Test | Expected result |
|---|---|---|
| valid | `manual_sample.tri` | manual 5.5 sample → TAC (chapter 10) |
| valid | `manual_tac_example.tri` | manual 4.6 example → exactly the TAC shown in the manual |
| valid | `arithmetic.tri` | precedence and float division |
| valid | `control.tri` | `while` and `if-else` with labels |
| valid | `nested_scopes.tri` | nested blocks, shadowing (`n@3`), `&& \|\| !`, `%`, int/float mixing, `if` without else |
| lexical | `invalid_character.tri` | `line 2: lexical error: invalid character '@'` |
| lexical | `invalid_identifier.tri` | `line 1: lexical error: invalid identifier (cannot start with a digit) '2count'` |
| lexical | `unterminated_comment.tri` | `line 2: lexical error: unterminated block comment` |
| syntax | `missing_semicolon.tri` | `line 1: syntax error: unexpected PRINT on line 2 (missing ';' at the end of line 1?)` |
| syntax | `missing_parenthesis.tri` | `line 2: syntax error: unexpected '{'` |
| syntax | `multiple_errors.tri` | two errors reported (recovery) |
| semantic | `undeclared.tri` | undeclared variable |
| semantic | `redeclaration.tri` | redeclaration |
| semantic | `scope_violation.tri` | scope violation |
| semantic | `type_mismatch.tri` | `bool b = 5 + 3.2;` (the manual's example) → type mismatch |
| semantic | `invalid_assignment.tri` | invalid assignment |
| semantic | `invalid_expression.tri` | invalid expression (`true && 1`) |
| semantic | `non_bool_condition.tri` | `while (x)` with int `x` |
| semantic | `multiple_errors.tri` | redeclaration and undeclared reported together |

The exact output of each test is in its `.expected` file.

**[Screenshot]** `make` building the project.
**[Screenshot]** `make test` showing all tests passing.
**[Screenshot]** `--all` on `examples/valid/manual_sample.tri`.
**[Screenshot]** one lexical, one syntax and one semantic error.

## 13. Conclusion

The project showed how much each phase depends on the previous one. Line numbers recorded by the lexer surface in semantic errors three phases later. Precedence decided in the parser is what makes the post-order TAC walk correct. The scope stack in the symbol table determines both which errors are reported and how variables are named in the TAC. Rejecting invalid programs well — the right line, the right category, no avalanche of follow-on errors — took as much care as compiling valid ones.

## 14. References

1. Lab manual: *Compiler Construction Lab: Project Manual*, Metropolitan University, Department of CSE.
2. A. V. Aho, M. S. Lam, R. Sethi, J. D. Ullman, *Compilers: Principles, Techniques, and Tools*, 2nd ed., Addison-Wesley, 2006 (chapters 2, 4, 6).
3. GNU Bison manual, https://www.gnu.org/software/bison/manual/
4. Flex manual, https://westes.github.io/flex/manual/
5. AI assistance (Claude) was used while restructuring the code and documentation, as permitted by manual section 10. All members reviewed the result and must be able to explain every part.
