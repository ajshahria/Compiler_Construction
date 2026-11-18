#include "ast/ast.h"
Node* makeNode(const char* k,const char* t,int l,Node* a,Node* b,Node* c){auto*n=new Node{k,t?t:"",l,{}};if(a)n->children.push_back(a);if(b)n->children.push_back(b);if(c)n->children.push_back(c);return n;}
void freeTree(Node*n){if(!n)return;for(auto*c:n->children)freeTree(c);delete n;}
