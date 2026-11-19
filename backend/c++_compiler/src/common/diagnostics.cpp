#include "diagnostics.h"
#include <iostream>

static int counts[3] = {0, 0, 0};

void reportError(Phase phase, int line, const std::string& msg) {
    static const char* names[] = {"Lexical", "Syntax", "Semantic"};
    counts[phase]++;
    std::cerr << names[phase] << " error [line " << line << "]: " << msg << "\n";
}

int errorCount(Phase phase) { return counts[phase]; }
int totalErrors() { return counts[0] + counts[1] + counts[2]; }
void resetErrors() { counts[0] = counts[1] = counts[2] = 0; }
