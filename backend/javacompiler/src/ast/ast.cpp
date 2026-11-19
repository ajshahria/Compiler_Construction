/*
 * ast.cpp - construction, destruction and printing of AST nodes.
 */
#include "ast/ast.h"

Node* makeNode(const char* kind, const char* text, int line, Node* a, Node* b, Node* c) {
    Node* node = new Node;
    node->kind = kind;
    node->text = text ? text : "";
    node->line = line;
    for (Node* child : {a, b, c})
        if (child) node->children.push_back(child);
    return node;
}

void freeTree(Node* node) {
    if (!node) return;
    for (Node* child : node->children) freeTree(child);
    delete node;
}

/* Readable node names for the AST listing. */
static std::string displayName(const Node* node) {
    const std::string& k = node->kind;
    if (k == "program")  return "Program";
    if (k == "block")    return "Block";
    if (k == "decl")     return "VarDecl";
    if (k == "assign")   return "Assign";
    if (k == "inc")      return "Increment";
    if (k == "dec")      return "Decrement";
    if (k == "print")    return "Print";
    if (k == "if")       return node->children.size() > 2 ? "IfElse" : "If";
    if (k == "while")    return "While";
    if (k == "for")      return "For";
    if (k == "do_while") return "DoWhile";
    if (k == "switch")   return "Switch";
    if (k == "cases")    return "Cases";
    if (k == "case")     return "Case";
    if (k == "default")  return "Default";
    if (k == "break")    return "Break";
    if (k == "binary")   return "BinaryOp";
    if (k == "unary")    return "UnaryOp";
    if (k == "literal")  return "Literal";
    if (k == "id")       return "Identifier";
    return k;
}

void printAst(Node* node, std::ostream& out, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; ++i) out << "  ";
    out << displayName(node);
    if (!node->text.empty()) out << " " << node->text;
    out << " (line " << node->line << ")\n";
    for (Node* child : node->children) {
        if (child) {
            printAst(child, out, depth + 1);
        } else {                      // empty slot of a `for` header
            for (int i = 0; i <= depth; ++i) out << "  ";
            out << "<empty>\n";
        }
    }
}
