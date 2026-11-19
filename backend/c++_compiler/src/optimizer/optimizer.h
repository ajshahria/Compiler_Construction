// optimizer.h - Phase 5: machine-independent optimization on TAC.
//   1. constant folding and constant propagation (within basic blocks)
//   2. branch simplification (ifFalse on a constant)
//   3. unreachable code removal (after goto)
//   4. dead temporary elimination and temp-copy collapsing
//   5. removal of jumps to the next label and of unused labels
#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "tac.h"

TacProgram optimize(const TacProgram& in);

#endif
