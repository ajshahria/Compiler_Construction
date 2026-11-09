# Project Report: MiniJava Compiler

## Introduction and objectives

MiniJava is an educational compiler front end for a small Java-like language. Its objectives are to demonstrate the interaction of lexical analysis, parsing, AST construction, nested symbol scopes, type checking, diagnostics, and intermediate-code generation.

## Scope

The accepted language is defined in `language-grammar.md`. It is not the full Java language. The compiler emits Three Address Code and does not generate JVM bytecode, machine code, or a runnable Java class.

## Architecture and phases

1. Flex lexer converts source characters into tokens.
2. Bison parser recognizes the supported grammar.
3. AST represents the program's meaningful constructs.
4. Symbol table tracks declared names, types, and nested scopes.
5. Semantic analyzer checks declaration and type rules.
6. TAC generator emits temporaries and labels for expressions and control flow.

## Testing and results

Populate `tests/valid`, `tests/lexical_errors`, `tests/syntax_errors`, and `tests/semantic_errors` with the team's test inputs and expected outputs. Include both successful TAC generation and clear diagnostics in the final report.

## Limitations and future work

This compiler intentionally excludes most Java features. A future extension could add methods and parameters, arrays, class fields, object-oriented semantics, and a JVM bytecode backend, each with corresponding grammar and type-system work.
