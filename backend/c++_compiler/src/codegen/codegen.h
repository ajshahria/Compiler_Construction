// codegen.h - Phase 6: TAC -> 32-bit x86 assembly (AT&T syntax, MinGW/Windows).
// int and bool live in 32-bit memory cells and use integer instructions;
// float uses the x87 FPU. Build the output with:  gcc -m32 out.s -o out.exe
// Bonus: an array is a block of 4-byte cells, element i at _v_a(,%ecx,4). A function is
// a routine _f_name after main: the caller pushes the arguments, the result comes back
// in %eax (a float as its bits), and the function saves its own locals on the stack on
// entry and restores them on exit, so recursion works with cells in memory.
#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include "tac.h"

std::string generateAssembly(const TacProgram& p);

#endif
