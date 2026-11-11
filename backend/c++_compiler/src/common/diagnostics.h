// diagnostics.h - one place where every phase reports errors.
#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <string>

enum Phase { PH_LEXICAL, PH_SYNTAX, PH_SEMANTIC };

// Prints "<Phase> error [line N]: msg" to stderr and counts it.
void reportError(Phase phase, int line, const std::string& msg);
int  errorCount(Phase phase);
int  totalErrors();
void resetErrors();

#endif
