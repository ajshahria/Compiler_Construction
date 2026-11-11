// codegen.h - Phase 6: TAC -> 32-bit x86 assembly (AT&T syntax, MinGW/Windows).
// int and bool live in 32-bit memory cells and use integer instructions;
// float uses the x87 FPU. Build the output with:  gcc -m32 out.s -o out.exe
#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include "tac.h"

std::string generateAssembly(const TacProgram& p);

#endif
