# MiniJava Compiler: Project Report

Compiler Construction Lab, Metropolitan University, Department of CSE
Component: compiler front-end with a Java flavour (Flex + Bison + C++), `backend/javacompiler`

This report follows the chapter structure in section 12 of the lab manual. Screenshots required by section 11 go in the places marked **[Screenshot]**.

---

## 1. Introduction

During the semester the compiler phases were studied separately. This project joins them into one working pipeline for the fixed language of the lab manual. This component also accepts the language in a Java style: a `class` with a `main` method, `boolean`, and `System.out.println`. It shows how the same front-end design applies to a Java-like surface syntax.

## 2. Objectives

- Tokenise the language with Flex and report invalid tokens with line numbers.
- Parse it with an unambiguous Bison grammar, report syntax errors, and recover from them.
- Build an AST and print it readably.
- Keep a symbol table with nested scopes.
- Enforce the semantic rules of manual 4.5, plus Java's "definitely initialized" rule.
- Generate Three Address Code for expressions, control flow and printing.
- Back every phase with test programs and expected outputs.

## 3. Language specification

**Types:** `int`, `float`, and `bool` (also spelled `boolean`).

**Statements:**
- declaration (`int x;` or `int x = e;`)
- assignment
- `print e;` or `System.out.println(e);`
- `if`, `if-else`, `while`
- nested blocks, each with its own scope
- bonus: `for`, `do-while`, `switch`/`case`/`default`/`break`, `x++`, `x--`

**Operators:** `+ - * / %`, `< > <= >= == !=`, `&& || !`, unary `-`.

**Program form:** the manual's statements inside `class Name { public static void main(String[] args) { ... } }`. A bare list of statements is a syntax error, so C or C++ programs are not accepted as Java.

The complete CFG, precedence table and lexical rules are in [`language-grammar.md`](language-grammar.md).

**Typing rules:**

| Construct | Operands | Result |
|---|---|---|
| `+ - * / %` | int/float | int if both are int, otherwise float |
| `< > <= >=` | int/float | boolean |
| `== !=` | two numbers or two booleans | boolean |
| `&& \|\|`, `!` | boolean | boolean |
| unary `-` | int/float | same type |
| assignment, initializer | value type equals variable type, or int stored into float | |
| `if`/`while`/`for`/`do-while` condition | boolean | |
| `switch` selector | int or boolean; each case label must have the same type | |

## 4. Compiler architecture

```text
 source (.java)
     |
     v
 Lexical analyzer (src/lexer/lexer.l) -- tokens + lines -->
 Syntax analyzer  (src/parser/parser.y, builds the AST in src/ast/) -- AST -->
 Semantic analyzer (src/semantic/ + src/symbol_table/) -- annotated AST -->
 TAC generator    (src/codegen/) --> Three Address Code
```

The driver, `src/main.cpp`, runs the phases in order and stops after the first phase that reports an error. Bison calls `yylex()` whenever it needs a token, so lexing and parsing run together. The semantic analyzer annotates each variable node with its unique TAC name, and the generator uses that name. The options `--tokens`, `--ast`, `--symtab`, `--tac` and `--all` show each phase's output. `--emit-files` writes `ast.txt`, `symbols.txt` and `output.tac`, which the GUI (`ui/main.cpp`) shows in separate tabs; the GUI can also execute the TAC.

## 5. Lexer design

- Comments and whitespace come first. `/*` enters an exclusive `COMMENT` start condition. Reaching end-of-file inside it produces *unterminated block comment*, reported at the line where the comment opened.
- Keywords are listed before the identifier rule. When both match the same text, Flex prefers the earlier rule. `bool` and `boolean` both return the `BOOLEAN` token. `print` returns `PRINT`, and `System.out.println` is recognised as a single `PRINTLN` token.
- Multi-character operators (`== != <= >= && || ++ --`) win over single characters by longest match.
- `[0-9]+\.[0-9]+(exponent)?` is a float literal and `[0-9]+` is an integer literal.
- `[0-9]+{ID}`, such as `2count`, is reported as an *invalid identifier*. It is then returned as an identifier, so the parser adds no misleading follow-on error.
- `.` catches every other character. It is reported as an *invalid character* and skipped, so all lexical errors in a file are found.

`YY_USER_ACTION` copies `yylineno` into `yylloc` for every token. A wrapper around the generated scanner also remembers the previous token and its line, which `yyerror` uses (chapter 6).

## 6. Parser design

The grammar uses one `expression` rule with Bison precedence declarations, from lowest: `||`, `&&`, `== !=`, `< > <= >=`, `+ -`, `* / %`, then unary `! -` with `%prec UMINUS`. Every binary operator is left-associative. The dangling else is resolved by giving `ELSE` a higher precedence than `LOWER_THAN_ELSE`. Bison reports **no conflicts**.

`program` has two alternatives: a bare statement list, or the class/main wrapper. The first token (`class` or not) tells the parser which one applies, so the choice adds no conflict.

**Error handling.** `%error-verbose` produces messages such as `unexpected '{'`. A missing `;` is only noticed at the next token, which is usually on the next line. When the offending token starts a new line and the previous line did not end in `;`, `{` or `}`, `yyerror` reports the previous line and adds a hint:

```text
line 3: syntax error: unexpected PRINTLN on line 4 (missing ';' at the end of line 3?)
```

The rules `statement: error ';' | error '}'` let the parser drop the broken statement and continue, so `tests/syntax/multiple_errors.java` reports both of its errors in one run.

## 7. Abstract Syntax Tree

A node (`src/ast/ast.h`) has a `kind`, a `text`, a `line`, its `children`, and a `tacName` filled in later by the semantic analyzer. The header comment of `ast.h` documents every kind. Nodes are created bottom-up in the Bison actions. Punctuation and the class wrapper are dropped. `--ast` prints readable names such as `VarDecl`, `Assign`, `IfElse`, `While`, `BinaryOp` and `Identifier`. For `c = a + b * 2;` it prints:

```text
Assign c (line 7)
  BinaryOp + (line 7)
    Identifier a (line 7)
    BinaryOp * (line 7)
      Identifier b (line 7)
      Literal 2 (line 7)
```

## 8. Semantic analysis

`src/semantic/semantic.cpp` walks the AST. It opens a scope for every block and for every `for` header.

| Rule | How it is detected | Message (example) |
|---|---|---|
| Undeclared variable | not visible, and never declared anywhere | `undeclared variable 'missing'` |
| Redeclaration | name already exists in the *current* scope | `redeclaration of 'value' (already declared at line 1 in this scope)` |
| Scope violation | not visible, but declared earlier in a block that has closed | `scope violation: 'local' (declared at line 2) is not visible outside its block` |
| Type mismatch | initializer or condition has the wrong type | `type mismatch: cannot initialize boolean variable 'b' with a float value` |
| Invalid assignment | value type not assignable to the variable | `invalid assignment: cannot assign a boolean value to int variable 'count'` |
| Invalid expression | operand types violate the typing table | `invalid expression: logical operator '&&' needs boolean operands, got boolean and int` |
| Uninitialized use (Java) | variable read before it is definitely assigned | `variable 'total' might not have been initialized` |
| `break` misplaced | `break` outside any loop or switch | `break is only valid inside a loop or switch` |
| `switch` checks | selector type, case label types, duplicate labels, more than one default | `duplicate case label '2'` |

**Definite initialization.** Before an `if` the analyzer copies the symbol table, then checks each branch separately and merges the copies. A variable counts as initialized after the `if` only if *both* branches initialize it. Loop bodies might run zero times, so their assignments are discarded after the loop. `do-while` bodies always run once, so theirs are kept.

An expression whose type cannot be determined gets the internal type `Error`, and no further errors are reported about it. This prevents one mistake from causing a chain of errors.

## 9. Symbol table

`src/symbol_table/` holds a vector of scopes, used as a stack. Each scope is a hash map from a name to its `SymbolInfo`: type, declaration line, scope id, TAC name, and whether it is initialized. Index 0 is the global scope.

- `declare` inserts into the innermost scope and fails if the name is already there.
- `lookup` searches from the innermost scope outwards, so inner declarations shadow outer ones.
- `enter`/`leave` push and pop scopes. Once a scope is popped, its names are no longer visible.

The analyzer also keeps a declaration history in program order. `--symtab` prints it, and it is how a scope violation is told apart from an undeclared name:

```text
Name            Type     Scope  Depth  Line   TAC name
n               int      0      0      3      n
square          int      1      1      8      square
n               int      3      2      13     n@3
```

## 10. Intermediate code

`src/codegen/tac.cpp` produces one instruction per line, with at most one operator per instruction: `x = y`, `t = a op b`, `t = op a`, `ifFalse c goto L`, `if c goto L`, `goto L`, `L:` and `print x`. Expressions are generated in post-order, and each result goes into a new temporary. `&&` and `||` short-circuit: the right operand is only evaluated when it can still change the result.

```text
if (c) S1 else S2          while (c) S              do S while (c);
  <c>                      L1:                      L1:
  ifFalse c goto L1          <c>                      <S>
  <S1>                       ifFalse c goto L2        <c>
  goto L2                    <S>                      if c goto L1
L1:                          goto L1                L2:
  <S2>                     L2:
L2:
```

`for` puts its update just before the jump back to the start. `switch` compares the selector with each case label, jumps to the first match (or to `default`), and lets cases fall through as in Java. `break` jumps to the end label of the innermost loop or switch.

All variables share one namespace in TAC. When a declaration reuses a name that was already declared, for example a variable shadowing an outer one, it gets the TAC name `name@scope`. For the manual's section 4.6 example the output matches the manual exactly: `a = 5`, `b = 10`, `t1 = b * 2`, `t2 = a + t1`, `c = t2`.

## 11. Challenges

| Challenge | Resolution |
|---|---|
| The compiler first accepted only a Java-shaped program, which the manual's sample program is not. | `program` gained a second alternative (a bare statement list), and `bool` and `print` were added. Both forms now compile. |
| The first syntax error aborted parsing (`YYABORT`), and an invalid character aborted everything. | Bison `error` recovery rules were added, and the lexer now reports invalid characters and skips them. |
| A missing `;` was reported on the wrong line. | Previous-token tracking in the lexer, plus the line heuristic in `yyerror`. |
| Definite initialization across branches and loops. | Copy the symbol table, check each path, then merge (chapter 8). |
| A shadowing variable overwrote the outer one in the TAC. | Unique TAC names (`n@3`). |
| Generated files were committed, and a nested `java_compiler/` folder did not match the manual's layout (section 8). | The folder was flattened to `src/`, `tests/`, `docs/` and `examples/`. Everything generated now lives in `build/`, which `.gitignore` excludes. |

## 12. Testing

`make test` runs `tests/run_tests.sh`. It compiles each `tests/<category>/*.java` file and compares the output and exit status with `<name>.expected`. Current result: **24 passed, 0 failed**.

| Category | Programs |
|---|---|
| valid (9) | `manual_sample` (manual 5.5), `manual_tac_example` (manual 4.6), `java_wrapper`, `nested_scopes`, `for_loop`, `do_while`, `switch_case`, `smoke`, `sum_three` |
| lexical (3) | `bad_character`, `invalid_identifier`, `unterminated_comment` |
| syntax (3) | `missing_semicolon`, `missing_parenthesis`, `multiple_errors` |
| semantic (9) | `undeclared`, `redeclaration`, `scope_violation`, `type_mismatch` (the manual's `bool b = 5 + 3.2;`), `invalid_assignment`, `invalid_expression`, `non_boolean_condition`, `uninitialized`, `break_outside_loop` |

**[Screenshot]** `make` building the project.
**[Screenshot]** `make test` with every test passing.
**[Screenshot]** `--all` on `tests/valid/manual_sample.java`.
**[Screenshot]** one lexical, one syntax and one semantic error.
**[Screenshot]** the GUI (`make ui`) showing the AST, symbol table and TAC tabs.

## 13. Conclusion

Each phase depends on the one before it. Line numbers recorded by the lexer reach the semantic error messages. Precedence decided in the parser makes the simple post-order TAC walk correct. The scope stack decides both which errors are reported and how variables are named in the TAC. Supporting the manual's form and the Java form required changes only to the lexer and the top of the grammar; every later phase works on the same AST.

## 14. References

1. *Compiler Construction Lab: Project Manual*, Metropolitan University, Department of CSE.
2. A. V. Aho, M. S. Lam, R. Sethi, J. D. Ullman, *Compilers: Principles, Techniques, and Tools*, 2nd ed., 2006.
3. J. Gosling et al., *The Java Language Specification*, chapter 16 (Definite Assignment).
4. GNU Bison manual, https://www.gnu.org/software/bison/manual/
5. Flex manual, https://westes.github.io/flex/manual/
6. AI assistance (Claude) was used while restructuring the code and documentation, as manual section 10 permits. Every member must be able to explain every part.
