# MiniJava Language Grammar

The parser accepts one class with a Java-style `main` method. This is a deliberately small teaching subset, not the Java grammar. Types are `int`, `float`, and `boolean`; blocks establish nested scopes.

```ebnf
program      = "class" IDENT "{" "public" "static" "void" "main" "(" "String" "[" "]" IDENT ")" block "}" ;
block        = "{" { statement } "}" ;
statement    = type IDENT [ "=" expression ] ";"
             | IDENT "=" expression ";"
             | "System" "." "out" "." "println" "(" expression ")" ";"
             | "if" "(" expression ")" statement [ "else" statement ]
             | "while" "(" expression ")" statement
             | block ;
type         = "int" | "float" | "boolean" ;
expression   = logical_or ;
logical_or   = logical_and { "||" logical_and } ;
logical_and  = equality { "&&" equality } ;
equality     = relation { ("==" | "!=") relation } ;
relation     = additive { ("<" | "<=" | ">" | ">=") additive } ;
additive     = multiplicative { ("+" | "-") multiplicative } ;
multiplicative = unary { ("*" | "/" | "%") unary } ;
unary        = ("!" | "-") unary | primary ;
primary      = INT_LITERAL | FLOAT_LITERAL | "true" | "false" | IDENT | "(" expression ")" ;
```

`System.out.println` is the sole supported output construct. Method calls, class members, object construction, arrays, imports, access modifiers beyond the entry point, and other Java syntax are outside the subset.
