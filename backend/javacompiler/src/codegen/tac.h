/*
 * tac.h - Three Address Code generation from the checked AST.
 */
#pragma once
#include "ast/ast.h"
#include <iosfwd>
void generateTac(Node* root,std::ostream& out);
