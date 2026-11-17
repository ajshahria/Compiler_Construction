# Shared lab language grammar

This is the CFG implemented by `src/parser/parser.y` (EBNF notation). Operator precedence, from lowest to highest, is `||`, `&&`, equality, relational, `+ -`, `* / %`, unary `! -`.

```ebnf
program       = { statement } ;
statement     = declaration | assignment | print | if-statement
              | while-statement | block | recovered-error ;
declaration   = type identifier [ "=" expression ] ";" ;
assignment    = identifier "=" expression ";" ;
print         = "print" expression ";" ;
if-statement  = "if" "(" expression ")" statement
                [ "else" statement ] ;
while-statement = "while" "(" expression ")" statement ;
block         = "{" { statement } "}" ;
type          = "int" | "float" | "bool" ;
expression    = literal | identifier | "(" expression ")"
              | "!" expression | "-" expression
              | expression binary-operator expression ;
literal       = integer | floating-point | "true" | "false" ;
binary-operator = "+" | "-" | "*" | "/" | "%" | "<" | "<=" | ">" | ">="
               | "==" | "!=" | "&&" | "||" ;
```

Lexical forms: identifiers begin with an ASCII letter or underscore and continue with letters, digits, or underscores. Integers are decimal digits. Floats have digits on both sides of a decimal point. Whitespace and `//` or `/* ... */` comments are ignored. Braces, parentheses, semicolons, and operators are punctuation/tokens.

The parser recovers at a semicolon after a syntax error so it can report later errors too. The semantic pass checks declarations, use-before-declaration, same-scope redeclaration, scope exit, assignment/initializer types, operator types, and boolean control-flow conditions.

