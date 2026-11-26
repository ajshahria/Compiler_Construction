/*
 * ast.cpp - construction, destruction and printing of AST nodes.
 */
#include "ast/ast.h"

#include <sstream>
#include <utility>
#include <vector>

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
    if (k == "scanner")  return "ScannerDecl";
    if (k == "read")     return "ScannerRead";
    if (k == "if")       return node->children.size() > 2 ? "IfElse" : "If";
    if (k == "while")    return "While";
    if (k == "for")      return "For";
    if (k == "do_while") return "DoWhile";
    if (k == "switch")   return "Switch";
    if (k == "cases")    return "Cases";
    if (k == "case")     return "Case";
    if (k == "default")  return "Default";
    if (k == "break")    return "Break";
    if (k == "continue") return "Continue";
    if (k == "array_decl")   return "ArrayDecl";
    if (k == "new_array")    return "NewArray";
    if (k == "array_init")   return "ArrayInit";
    if (k == "index")        return "ArrayIndex";
    if (k == "assign_index") return "AssignElement";
    if (k == "length")       return "ArrayLength";
    if (k == "method")       return "Method";
    if (k == "params")       return "Params";
    if (k == "param")        return "Param";
    if (k == "call")         return "Call";
    if (k == "call_stmt")    return "CallStatement";
    if (k == "return")       return "Return";
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
    if (!node->type.empty()) out << " " << node->type;      // a method's return type
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

/* Fill colour of a node in the --dot graph, by the kind of construct. */
static const char* dotColour(const std::string& label) {
    static const std::pair<const char*, const char*> kinds[] = {
        {"Program", "#d9d2e9"}, {"Method", "#d9d2e9"}, {"Params", "#d9d2e9"}, {"Param", "#d9d2e9"},
        {"VarDecl", "#cfe2f3"}, {"ArrayDecl", "#cfe2f3"}, {"ScannerDecl", "#cfe2f3"},
        {"Assign", "#d9ead3"}, {"AssignElement", "#d9ead3"}, {"Increment", "#d9ead3"}, {"Decrement", "#d9ead3"},
        {"If", "#fce5cd"}, {"IfElse", "#fce5cd"}, {"While", "#fce5cd"}, {"For", "#fce5cd"},
        {"DoWhile", "#fce5cd"}, {"Switch", "#fce5cd"}, {"Case", "#fce5cd"}, {"Default", "#fce5cd"},
        {"Break", "#fce5cd"}, {"Continue", "#fce5cd"}, {"Return", "#fce5cd"},
        {"BinaryOp", "#fff2cc"}, {"UnaryOp", "#fff2cc"},
        {"Literal", "#ffffff"}, {"Identifier", "#ffffff"}, {"ArrayIndex", "#ffffff"}, {"ArrayLength", "#ffffff"},
        {"Call", "#ead1dc"}, {"CallStatement", "#ead1dc"},
    };
    std::string word = label.substr(0, label.find_first_of(" ("));
    for (const auto& k : kinds)
        if (word == k.first) return k.second;
    return "#f3f3f3";
}

/* --dot: the --ast listing as a Graphviz digraph. Every line of the listing
 * is a node; its parent is the nearest line above it indented one level
 * less. Render with:  dot -Tpng ast.dot -o ast.png */
void printDot(Node* root, std::ostream& out) {
    std::ostringstream listing;
    printAst(root, listing);
    std::istringstream lines(listing.str());
    std::vector<int> parents;               // node number at each depth
    std::string line;
    int node = 0;
    out << "digraph AST {\n"
        << "    node [shape=box, style=\"rounded,filled\", fontname=\"Helvetica\", fontsize=11];\n"
        << "    edge [arrowsize=0.7];\n";
    while (std::getline(lines, line)) {
        std::size_t depth = 0;
        while (line.compare(depth * 2, 2, "  ") == 0) ++depth;
        std::string label = line.substr(depth * 2), escaped;
        for (char c : label) {
            if (c == '"' || c == '\\') escaped += '\\';
            escaped += c;
        }
        out << "    n" << node << " [label=\"" << escaped << "\", fillcolor=\"" << dotColour(label) << "\"];\n";
        if (parents.size() <= depth) parents.resize(depth + 1);
        parents[depth] = node;
        if (depth > 0) out << "    n" << parents[depth - 1] << " -> n" << node << ";\n";
        ++node;
    }
    out << "}\n";
}
