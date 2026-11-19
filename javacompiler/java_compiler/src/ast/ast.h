#pragma once
#include <string>
#include <vector>
struct Node { std::string kind, text; int line=1; std::vector<Node*> children; };
Node* makeNode(const char* kind, const char* text, int line, Node* a=nullptr, Node* b=nullptr, Node* c=nullptr);
void freeTree(Node* n);
