# Test inputs

Run the valid cases and confirm the lexer, AST, and IR commands finish successfully. Run the invalid cases and confirm the compiler reports source line numbers and exits with an error. These are demonstration inputs; expand them when you add features.

```sh
./build/tricora --tokens tests/valid_arithmetic.tri
./build/tricora --ast tests/valid_control.tri
./build/tricora --ir tests/valid_control.tri
./build/tricora tests/invalid_lexical.tri
./build/tricora tests/invalid_syntax.tri
./build/tricora tests/invalid_semantic.tri
./build/tricora tests/invalid_redeclaration.tri
./build/tricora tests/invalid_assignment.tri
./build/tricora tests/invalid_operator.tri
./build/tricora tests/invalid_scope.tri
```

The user-requested lab rubric expects valid and invalid test programs and screenshots/output evidence. Include your own run results in the report.

