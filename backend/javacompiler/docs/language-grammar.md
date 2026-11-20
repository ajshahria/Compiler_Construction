# MiniJava grammar

This is the context-free grammar implemented by `src/parser/parser.y`. Its core is exactly the language in section 5 of the lab manual. On top of that it allows:

- the Java spellings `boolean` and `System.out.println(...)`,
- a required class/main wrapper (a bare list of statements is rejected),
- the bonus statements of manual section 14: `for`, `do-while`, `switch`, `break`, `++` and `--`.

## Context-free grammar (BNF)

`ε` is the empty string. Terminals are quoted or written in capitals.

```text
<program>      ::= "class" IDENT "{" "public" "static" "void" "main"
                   "(" "String" "[" "]" IDENT ")" <block> "}"

<block>        ::= "{" <stmt-list> "}"
<stmt-list>    ::= <stmt-list> <statement> | ε

<type>         ::= "int" | "float" | "bool" | "boolean"

<statement>    ::= <type> IDENT ";"                                 (declaration)
                 | <type> IDENT "=" <expr> ";"                      (declaration with initializer)
                 | IDENT "=" <expr> ";"                             (assignment)
                 | "print" <expr> ";"                               (print, manual form)
                 | "System.out.println" "(" <expr> ")" ";"          (print, Java form)
                 | "if" "(" <expr> ")" <statement>
                 | "if" "(" <expr> ")" <statement> "else" <statement>
                 | "while" "(" <expr> ")" <statement>
                 | <block>
                 -- bonus --
                 | IDENT "++" ";" | IDENT "--" ";"
                 | "for" "(" <for-init> ";" <opt-expr> ";" <for-update> ")" <statement>
                 | "do" <statement> "while" "(" <expr> ")" ";"
                 | "switch" "(" <expr> ")" "{" <case-list> "}"
                 | "break" ";"

<for-init>     ::= ε | IDENT "=" <expr> | <type> IDENT "=" <expr>
<for-update>   ::= ε | IDENT "=" <expr> | IDENT "++" | IDENT "--"
<opt-expr>     ::= ε | <expr>
<case-list>    ::= <case-list> <case> | ε
<case>         ::= "case" INT_LITERAL ":" <stmt-list>
                 | "case" "true" ":" <stmt-list> | "case" "false" ":" <stmt-list>
                 | "default" ":" <stmt-list>

<expr>         ::= <expr> "||" <and-expr>      | <and-expr>
<and-expr>     ::= <and-expr> "&&" <eq-expr>   | <eq-expr>
<eq-expr>      ::= <eq-expr> "==" <rel-expr>   | <eq-expr> "!=" <rel-expr>   | <rel-expr>
<rel-expr>     ::= <rel-expr> "<" <add-expr>   | <rel-expr> ">" <add-expr>
                 | <rel-expr> "<=" <add-expr>  | <rel-expr> ">=" <add-expr>  | <add-expr>
<add-expr>     ::= <add-expr> "+" <mul-expr>   | <add-expr> "-" <mul-expr>   | <mul-expr>
<mul-expr>     ::= <mul-expr> "*" <unary>      | <mul-expr> "/" <unary>
                 | <mul-expr> "%" <unary>      | <unary>
<unary>        ::= "!" <unary> | "-" <unary> | <primary>
<primary>      ::= INT_LITERAL | FLOAT_LITERAL | "true" | "false" | IDENT | "(" <expr> ")"
```

The layered `<expr>` rules spell out precedence. `parser.y` writes the same language as one `expression` rule plus Bison precedence declarations, which accept exactly the same programs.

## Precedence and associativity (lowest first)

| Level | Operators | Associativity |
|---|---|---|
| 1 | `\|\|` | left |
| 2 | `&&` | left |
| 3 | `==` `!=` | left |
| 4 | `<` `>` `<=` `>=` | left |
| 5 | `+` `-` | left |
| 6 | `*` `/` `%` | left |
| 7 | unary `!` `-` | right |

**Dangling else.** The token `ELSE` has a higher precedence than the rule `if ( expr ) statement` (`%prec LOWER_THAN_ELSE`). The parser therefore shifts `else` and attaches it to the nearest `if`. Bison reports no conflicts for the grammar.

## Lexical rules

| Token | Pattern |
|---|---|
| IDENT | `[A-Za-z_][A-Za-z0-9_]*` (excluding keywords) |
| INT_LITERAL | `[0-9]+` |
| FLOAT_LITERAL | `[0-9]+\.[0-9]+` with an optional exponent such as `e10` |
| keywords | `int float bool boolean if else while print true false for do switch case default break class public static void main String System.out.println` |
| operators | `+ - * / % < > <= >= == != && \|\| ! = ++ --` |
| delimiters | `{ } ( ) [ ] ; :` |
| comments (discarded) | `//` to end of line, `/* ... */` (may span lines) |
| whitespace (discarded) | spaces, tabs, carriage returns, newlines |

Anything else is a lexical error: an invalid character, a digit followed by letters (`2count`), or a `/*` comment that is never closed.

## Error recovery

```text
<statement> ::= error ";" | error "}"
```

After a syntax error the parser discards tokens up to the next `;` or `}` and continues parsing.
