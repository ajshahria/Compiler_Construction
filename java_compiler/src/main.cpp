#include "ast/ast.h"
#include "semantic/semantic.h"
#include "codegen/tac.h"
#include <cstdio>
#include <iostream>
extern FILE* yyin; extern int yyparse(); extern Node* syntaxRoot;
int main(int argc,char**argv){if(argc!=2){std::cerr<<"usage: minijavac <source.java>\n";return 2;}yyin=fopen(argv[1],"r");if(!yyin){std::cerr<<"cannot open input: "<<argv[1]<<'\n';return 2;}int parsed=yyparse();fclose(yyin);if(parsed||!syntaxRoot)return 1;if(!analyze(syntaxRoot)){freeTree(syntaxRoot);return 1;}generateTac(syntaxRoot,std::cout);freeTree(syntaxRoot);return 0;}
