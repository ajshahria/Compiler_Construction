// optimizer.h - Phase 5: machine-independent optimization on TAC.
//   1. constant folding and constant propagation (within basic blocks)
//   2. branch simplification (ifFalse on a constant)
//   3. unreachable code removal (after goto)
//   4. dead temporary elimination and temp-copy collapsing
//   5. removal of jumps to the next label and of unused labels
// Calls, array elements and function boundaries are respected: what is known
// about variables is forgotten after a call (it may change globals) and at the
// start and end of a function; a call is kept even when its value is unused.
#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "tac.h"

TacProgram optimize(const TacProgram& in);

#endif
