/*
 * semantic.h - semantic analysis (scope and type checking) of the AST.
 */
#pragma once

#include "ast/ast.h"

#include <string>

/* Checks the whole program and prints one message per error to stderr.
 * Returns the number of semantic errors (0 = the program is valid). */
int analyze(Node* root);

/* Table of every declared variable: name, type, scope, depth, line, TAC name
 * (the --symtab option). Valid after analyze(). */
std::string symbolTableReport();
