# Formal Grammar (CFG)

Terminals are in `monospace`; `ε` is the empty string. The grammar is implemented in
[`src/parser/parser.y`](../src/parser/parser.y).

## Core language (manual §5)

```
program      → stmt_list
stmt_list    → ε | stmt_list stmt
stmt         → decl | assign_stmt | if_stmt | while_stmt | print_stmt | block | ';'
block        → '{' stmt_list '}'
type         → 'int' | 'float' | 'bool'
decl         → type ID ';'  |  type ID '=' expr ';'
assign_stmt  → ID '=' expr ';'
if_stmt      → 'if' '(' expr ')' stmt
             | 'if' '(' expr ')' stmt 'else' stmt
while_stmt   → 'while' '(' expr ')' stmt
print_stmt   → 'print' expr ';'

expr         → expr '||' expr | expr '&&' expr
             | expr '==' expr | expr '!=' expr
             | expr '<' expr  | expr '>' expr | expr '<=' expr | expr '>=' expr
             | expr '+' expr  | expr '-' expr
             | expr '*' expr  | expr '/' expr | expr '%' expr
             | '!' expr | '-' expr | '(' expr ')'
             | INT_LIT | FLOAT_LIT | 'true' | 'false' | ID
```

The `expr` rule is ambiguous as written; Bison resolves it with precedence declarations.

| Level (low → high) | Operators | Associativity |
|---|---|---|
| 1 | `\|\|` | left |
| 2 | `&&` | left |
| 3 | `==` `!=` | left |
| 4 | `<` `>` `<=` `>=` | left |
| 5 | `+` `-` | left |
| 6 | `*` `/` `%` | left |
| 7 | `!` unary `-` | right |

**Dangling else:** `if (a) if (b) x; else y;` – the `else` belongs to the nearest `if`
(rule `if` without else has the lower precedence `LOWER_THAN_ELSE`, so Bison shifts `else`).

## Lexical elements

```
ID        → [A-Za-z_][A-Za-z0-9_]*
INT_LIT   → [0-9]+
FLOAT_LIT → [0-9]+ '.' [0-9]* | '.' [0-9]+ | (either of those or [0-9]+) followed by an optional exponent e[+-]?[0-9]+
STRING_LIT → "..." | '.'      with the escapes \n \t \r \0 \\ \" \'    (only allowed as a cout item)
comments  → '//' … end of line   |   '/*' … '*/'     (discarded)
```

## C++ syntax on top of the core

```
stmt          → ... | do_stmt | for_stmt | return_stmt | main_def | switch_stmt
              | cout_stmt | cin_stmt | 'break' ';' | 'continue' ';'
main_def      → 'int' 'main' '(' ')' block | 'int' 'main' '(' 'void' ')' block | 'void' 'main' '(' ')' block
type          → 'int' | 'float' | 'double' | 'bool' | 'long' | 'long' 'long' | 'short'
decl          → ['const'] type declarators ';'
declarators   → declarator | declarators ',' declarator
declarator    → ID | ID '=' expr
simple_assign → ID '=' expr | ID ('+=' | '-=' | '*=' | '/=') expr | ID '++' | ID '--' | '++' ID | '--' ID
assign_stmt   → simple_assign ';'
do_stmt       → 'do' stmt 'while' '(' expr ')' ';'
for_stmt      → 'for' '(' for_init ';' for_cond ';' for_step ')' stmt
for_init      → ε | simple_assign | type ID '=' expr
for_cond      → ε | expr                 (ε means true)
for_step      → ε | simple_assign
switch_stmt   → 'switch' '(' expr ')' '{' case_list '}'
case_list     → ε | case_list 'case' case_value ':' stmt_list | case_list 'default' ':' stmt_list
case_value    → INT_LIT | '-' INT_LIT
return_stmt   → 'return' ';' | 'return' expr ';'
cout_stmt     → 'cout' '<<' cout_list ';'
cout_list     → cout_item | cout_list '<<' cout_item
cout_item     → expr | STRING_LIT | 'endl'
cin_stmt      → 'cin' '>>' cin_list ';'
cin_list      → ID | cin_list '>>' ID
expr          → ... | expr '?' expr ':' expr        (lowest precedence, right associative)
```

`x += e`, `x++` and `++x` are rewritten into `x = x op e`; `for` becomes a `while` node that carries
the update step, so `continue` runs the step. `cout`, `cin`, `break`, `continue`, `return` and `switch` are
translated to labels and jumps in the Three Address Code.

## Error recovery

`stmt → error ';'` skips to the end of a broken statement and `block → '{' stmt_list error '}'`
skips to the end of a broken block, so several syntax errors can be reported in one run.
