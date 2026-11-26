/*
 * semantic.h - semantic analysis (scope and type checking) of the AST.
 */
#pragma once

#include "ast/ast.h"
#include "common/types.h"

#include <string>
#include <utility>
#include <vector>

/* Checks the whole program and prints one message per error to stderr.
 * Returns the number of semantic errors (0 = the program is valid). */
int analyze(Node* root);

/* Table of every declared variable: name, type, scope, depth, line, TAC name
 * (the --symtab option). Valid after analyze(). */
std::string symbolTableReport();

/* Every declared name, for the back end: the TAC name, the type (a
 * method's return type), the number of elements of an array, whether it is
 * a method, and the method whose local it is (empty for main's variables). */
struct DeclaredName {
    std::string tacName;
    Type type;
    int length;
    bool isFunction;
    std::string function;
};

/* Valid after analyze(). */
std::vector<DeclaredName> declaredVariables();
