// interpreter.h - executes TAC directly so the compiled program can be run and checked.
#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <istream>
#include <ostream>
#include "tac.h"

// Runs the program: output goes to os, 'cin' input comes from is.
// Returns false on a runtime error.
bool interpret(const TacProgram& p, std::ostream& os, std::istream& is);

#endif
