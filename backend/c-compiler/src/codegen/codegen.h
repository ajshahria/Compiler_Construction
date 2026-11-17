#ifndef TRICORA_CODEGEN_H
#define TRICORA_CODEGEN_H
#include "ast.h"
#include <stdio.h>
void generate_ir(StmtList *program, FILE *out);
#endif

