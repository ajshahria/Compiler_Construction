# Grammar of the Tricora mini language

This is the context-free grammar of the language in section 5 of the lab manual. `src/parser/parser.y` implements it.

## Context-free grammar (BNF)

Nonterminals are written in `<angle brackets>` and terminals in **bold** or as quoted symbols. `ε` is the empty string.

```text
<program>        ::= <stmt-list>

<stmt-list>      ::= <stmt-list> <statement>
                   | ε

<statement>      ::= <type> IDENT ";"                              (declaration)
                   | <type> IDENT "=" <expr> ";"                   (declaration with initializer)
                   | IDENT "=" <expr> ";"                          (assignment)
                   | "print" <expr> ";"                            (print)
                   | "if" "(" <expr> ")" <statement>               (if)
                   | "if" "(" <expr> ")" <statement> "else" <statement>   (if-else)
                   | "while" "(" <expr> ")" <statement>            (while)
                   | <block>

<block>          ::= "{" <stmt-list> "}"

<type>           ::= "int" | "float" | "bool"

<expr>           ::= <expr> "||" <and-expr>       | <and-expr>
<and-expr>       ::= <and-expr> "&&" <eq-expr>    | <eq-expr>
<eq-expr>        ::= <eq-expr> "==" <rel-expr>    | <eq-expr> "!=" <rel-expr>    | <rel-expr>
<rel-expr>       ::= <rel-expr> "<" <add-expr>    | <rel-expr> ">" <add-expr>
                   | <rel-expr> "<=" <add-expr>   | <rel-expr> ">=" <add-expr>   | <add-expr>
<add-expr>       ::= <add-expr> "+" <mul-expr>    | <add-expr> "-" <mul-expr>    | <mul-expr>
<mul-expr>       ::= <mul-expr> "*" <unary-expr>  | <mul-expr> "/" <unary-expr>
                   | <mul-expr> "%" <unary-expr>  | <unary-expr>
<unary-expr>     ::= "!" <unary-expr> | "-" <unary-expr> | <primary>
<primary>        ::= INT_LITERAL | FLOAT_LITERAL | "true" | "false" | IDENT | "(" <expr> ")"
```

The layered `<expr>` rules state precedence and associativity directly in the grammar. In `parser.y` the same language is written more compactly as one `expression` rule plus Bison precedence declarations (see below). The two forms accept exactly the same programs.

## Precedence and associativity

From lowest to highest:

| Level | Operators | Associativity |
|---|---|---|
| 1 | `\|\|` | left |
| 2 | `&&` | left |
| 3 | `==` `!=` | left |
| 4 | `<` `>` `<=` `>=` | left |
| 5 | `+` `-` | left |
| 6 | `*` `/` `%` | left |
| 7 | unary `!` `-` | right |

## Dangling else

In `if (a) if (b) S1 else S2`, the `else` belongs to the nearest `if`. In Bison this is done by giving the token `ELSE` a higher precedence than the rule `if ( expr ) statement`, so the parser shifts the `else` instead of reducing. Bison reports **no shift/reduce or reduce/reduce conflicts** for the grammar.

## Lexical rules

| Token | Regular expression / text |
|---|---|
| IDENT | `[A-Za-z_][A-Za-z0-9_]*` (excluding keywords) |
| INT_LITERAL | `[0-9]+` |
| FLOAT_LITERAL | `[0-9]+\.[0-9]+` |
| keywords | `int float bool if else while print true false` |
| operators | `+ - * / % < > <= >= == != && \|\| ! =` |
| delimiters | `{ } ( ) ;` |
| comments (discarded) | `//` to end of line, `/* ... */` (may span lines) |
| whitespace (discarded) | spaces, tabs, carriage returns, newlines |

Anything else is a lexical error: an invalid character, a digit followed by letters (`2count`), or a `/*` comment that is never closed.

## Error recovery

```text
<statement> ::= error ";" | error "}"
```

After a syntax error the parser discards tokens up to the next `;` or `}` and continues, so one run reports every independent syntax error.

## C style (bonus, manual section 14)

A program may also be written as ordinary C. These rules are added to the grammar above:

```text
<statement>      ::= <type> IDENT "(" ")" <block>                  (function: only int main())
                   | <type> IDENT "(" "void" ")" <block>
                   | "return" <expr> ";"                           (return, only in main)
                   | "return" ";"
                   | "printf" "(" STRING_LITERAL <printf-args> ")" ";"

<printf-args>    ::= <printf-args> "," <expr>
                   | ε
```

| Token | Regular expression / text |
|---|---|
| STRING_LITERAL | `\"(\\.\|[^"\\\n])*\"` (text in double quotes on one line); the escapes `\n` `\t` `\r` `\0` `\\` `\"` `\'` are allowed |
| keywords | `return void printf` |
| delimiter | `,` |
| `#include <header>` | read by the lexer, no token. Accepted headers: `assert.h ctype.h errno.h float.h limits.h math.h stdbool.h stddef.h stdint.h stdio.h stdlib.h string.h time.h`. Any other header, a `"user.h"` header or another `#` directive is a lexical error |

The grammar alone cannot express these rules, so the semantic analyzer checks them:

- Only one function may be defined: `int main()` (or `int main(void)`), at the top level. When there is a `main`, only variable declarations (globals) may stand outside it.
- `return` is only allowed inside `main`, and its value must be an int. The value is not used.
- `printf` needs `#include <stdio.h>`. Its format may use `%d` and `%i` (an int or a bool, which prints as 0 or 1), `%f`, `%.Nf` and `%g` (a float) and `%%`. The number and types of the arguments must match the conversions.

Without `main`, a program is the manual's plain list of statements, exactly as before.

In the Three Address Code, a `printf` is split into output instructions, and `return` jumps to a label at the end of `main`:

```text
printf("x = %d, half = %.1f\n", x, half);      outs "x = "
                                               out x
                                               outs ", half = "
                                               outf half 1
                                               outs "\n"
```
