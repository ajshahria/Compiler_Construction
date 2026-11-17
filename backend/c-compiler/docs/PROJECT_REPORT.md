# Written report guide

Use this order for the individual C compiler component of the group report. Replace prompts with your own implementation details and screenshots.

1. **Introduction and motivation** — Tricora group project context and the C compiler component.
2. **Objectives** — demonstrate lexical, syntax, and semantic analysis and intermediate code generation.
3. **Language specification** — supported tokens, types, statements, expression precedence, and `docs/grammar.md` CFG.
4. **Compiler architecture** — source → Flex lexer → Bison parser → AST → semantic analysis/symbol table → three-address code.
5. **Lexer design** — token classes, identifier/number patterns, comment handling, invalid character diagnostics.
6. **Parser design** — grammar, precedence/associativity, AST construction, line-numbered syntax reporting and recovery.
7. **AST design** — node categories, how the parser builds the tree, and one sample tree.
8. **Semantic analysis** — each implemented rule and a small example showing its diagnostic.
9. **Symbol table design** — stored name/type/scope data, insertion, lookup, and block entry/exit.
10. **Intermediate code generation** — temporaries, labels, arithmetic/logical operations, `if`, `if-else`, `while`, and `print`.
11. **Challenges and solutions** — actual issues encountered and how your team resolved them.
12. **Testing** — commands and results for every valid/invalid input under `tests/`.
13. **Conclusion** — lessons learned.
14. **References** — external resources actually used.

The manual expects the team to understand every submitted part and each member to explain any part during the individual viva. Keep your own notes and make incremental Git commits that accurately reflect who did what.

