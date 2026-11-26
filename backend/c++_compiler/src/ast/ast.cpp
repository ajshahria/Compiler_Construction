// ast.cpp - indented text printer for the AST.
#include "ast.h"
#include <sstream>
#include <utility>

static void pad(std::ostream& os, int n) {
    for (int i = 0; i < n; i++) os << "  ";
}

static void printOpt(const Node* n, std::ostream& os, int indent) {
    if (n) n->print(os, indent);
    else { pad(os, indent); os << "(empty)\n"; }
}

void BlockNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << (kind == NodeKind::Program ? "Program" : kind == NodeKind::DeclGroup ? "DeclGroup" : "Block") << "\n";
    for (size_t i = 0; i < stmts.size(); i++) stmts[i]->print(os, indent + 1);
}

void VarDeclNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    if (length != 0) {
        os << "ArrayDecl " << typeName(declType) << " " << name << "[" << length << "] (line " << line << ")\n";
        for (size_t i = 0; i < initList.size(); i++) initList[i]->print(os, indent + 1);
        return;
    }
    os << "VarDecl " << typeName(declType) << " " << name << " (line " << line << ")\n";
    if (init) init->print(os, indent + 1);
}

void AssignNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    if (index) {
        os << "AssignElement " << name << " (line " << line << ")\n";
        pad(os, indent + 1); os << "Index:\n";
        index->print(os, indent + 2);
        pad(os, indent + 1); os << "Value:\n";
        value->print(os, indent + 2);
        return;
    }
    os << "Assign " << name << " (line " << line << ")\n";
    value->print(os, indent + 1);
}

void IndexNode::print(std::ostream& os, int indent) const {
    pad(os, indent); os << "ArrayIndex " << name << "\n";
    index->print(os, indent + 1);
}

void CallNode::print(std::ostream& os, int indent) const {
    pad(os, indent); os << "Call " << name << " (line " << line << ")\n";
    for (size_t i = 0; i < args.size(); i++) args[i]->print(os, indent + 1);
}

void FunctionNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Function " << typeName(retType) << " " << name << "(";
    for (size_t i = 0; i < params.size(); i++)
        os << (i ? ", " : "") << typeName(params[i].type) << " " << params[i].name;
    os << ") (line " << line << ")\n";
    body->print(os, indent + 1);
}

void IfNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "If (line " << line << ")\n";
    pad(os, indent + 1); os << "Condition:\n";
    cond->print(os, indent + 2);
    pad(os, indent + 1); os << "Then:\n";
    printOpt(thenStmt, os, indent + 2);
    if (elseStmt) {
        pad(os, indent + 1); os << "Else:\n";
        elseStmt->print(os, indent + 2);
    }
}

void WhileNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "While (line " << line << ")\n";
    pad(os, indent + 1); os << "Condition:\n";
    cond->print(os, indent + 2);
    pad(os, indent + 1); os << "Body:\n";
    printOpt(body, os, indent + 2);
    if (step) {
        pad(os, indent + 1); os << "Step (for-loop update):\n";
        step->print(os, indent + 2);
    }
}

void DoWhileNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "DoWhile (line " << line << ")\n";
    pad(os, indent + 1); os << "Body:\n";
    printOpt(body, os, indent + 2);
    pad(os, indent + 1); os << "Condition:\n";
    cond->print(os, indent + 2);
}

void JumpNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << (kind == NodeKind::Break ? "Break" : "Continue") << " (line " << line << ")\n";
}

void ReturnNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Return (line " << line << ")\n";
    if (value) value->print(os, indent + 1);
}

void SwitchNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Switch (line " << line << ")\n";
    expr->print(os, indent + 1);
    for (size_t i = 0; i < cases.size(); i++) {
        pad(os, indent + 1);
        if (cases[i].isDefault) os << "Default:\n";
        else os << "Case " << cases[i].value << ":\n";
        cases[i].body->print(os, indent + 2);
    }
}

void PrintNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Print (line " << line << ")\n";
    expr->print(os, indent + 1);
}

void CoutNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Cout (line " << line << ")\n";
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].kind == 0) items[i].expr->print(os, indent + 1);
        else if (items[i].kind == 1) { pad(os, indent + 1); os << "Text \"" << items[i].text << "\"\n"; }
        else if (items[i].kind == 3) {
            pad(os, indent + 1); os << "TextChoice \"" << items[i].text << "\" or \"" << items[i].text2 << "\"\n";
            items[i].expr->print(os, indent + 2);
        }
        else { pad(os, indent + 1); os << "Endl\n"; }
    }
}

void CinNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Cin (line " << line << ")\n";
    for (size_t i = 0; i < vars.size(); i++) vars[i]->print(os, indent + 1);
}

void BinaryNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "BinaryOp '" << op << "'\n";
    left->print(os, indent + 1);
    right->print(os, indent + 1);
}

void UnaryNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "UnaryOp '" << op << "'\n";
    operand->print(os, indent + 1);
}

void TernaryNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Ternary '?:'\n";
    cond->print(os, indent + 1);
    a->print(os, indent + 1);
    b->print(os, indent + 1);
}

void IntLitNode::print(std::ostream& os, int indent) const {
    pad(os, indent); os << "IntLit " << value << "\n";
}

void FloatLitNode::print(std::ostream& os, int indent) const {
    pad(os, indent); os << "FloatLit " << value << "\n";
}

void BoolLitNode::print(std::ostream& os, int indent) const {
    pad(os, indent); os << "BoolLit " << (value ? "true" : "false") << "\n";
}

void IdentNode::print(std::ostream& os, int indent) const {
    pad(os, indent); os << "Ident " << name << "\n";
}

/* Fill colour of a node in the --dot graph, by the kind of construct. */
static const char* dotColour(const std::string& label) {
    static const std::pair<const char*, const char*> kinds[] = {
        std::make_pair("Program", "#d9d2e9"), std::make_pair("Function", "#d9d2e9"),
        std::make_pair("VarDecl", "#cfe2f3"), std::make_pair("ArrayDecl", "#cfe2f3"),
        std::make_pair("Assign", "#d9ead3"), std::make_pair("AssignElement", "#d9ead3"),
        std::make_pair("If", "#fce5cd"), std::make_pair("While", "#fce5cd"), std::make_pair("DoWhile", "#fce5cd"),
        std::make_pair("Switch", "#fce5cd"), std::make_pair("Case", "#fce5cd"), std::make_pair("Default:", "#fce5cd"),
        std::make_pair("Break", "#fce5cd"), std::make_pair("Continue", "#fce5cd"), std::make_pair("Return", "#fce5cd"),
        std::make_pair("BinaryOp", "#fff2cc"), std::make_pair("UnaryOp", "#fff2cc"), std::make_pair("Ternary", "#fff2cc"),
        std::make_pair("IntLit", "#ffffff"), std::make_pair("FloatLit", "#ffffff"), std::make_pair("BoolLit", "#ffffff"),
        std::make_pair("Ident", "#ffffff"), std::make_pair("ArrayIndex", "#ffffff"),
        std::make_pair("Call", "#ead1dc"), std::make_pair("Cout", "#ead1dc"), std::make_pair("Cin", "#ead1dc"),
        std::make_pair("Print", "#ead1dc"),
    };
    std::string word = label.substr(0, label.find_first_of(" ("));
    for (size_t k = 0; k < sizeof kinds / sizeof kinds[0]; k++)
        if (word == kinds[k].first) return kinds[k].second;
    return "#f3f3f3";
}

/* --dot: the --ast listing as a Graphviz digraph. Every line of the listing
 * is a node; its parent is the nearest line above it indented one level
 * less. Render with:  dot -Tpng ast.dot -o ast.png */
void printDot(const Node* root, std::ostream& os) {
    std::ostringstream listing;
    root->print(listing, 0);
    std::istringstream lines(listing.str());
    std::vector<int> parents;               // node number at each depth
    std::string line;
    int node = 0;
    os << "digraph AST {\n"
       << "    node [shape=box, style=\"rounded,filled\", fontname=\"Helvetica\", fontsize=11];\n"
       << "    edge [arrowsize=0.7];\n";
    while (std::getline(lines, line)) {
        size_t depth = 0;
        while (line.compare(depth * 2, 2, "  ") == 0) ++depth;
        std::string label = line.substr(depth * 2), escaped;
        for (size_t i = 0; i < label.size(); i++) {
            if (label[i] == '"' || label[i] == '\\') escaped += '\\';
            escaped += label[i];
        }
        os << "    n" << node << " [label=\"" << escaped << "\", fillcolor=\"" << dotColour(label) << "\"];\n";
        if (parents.size() <= depth) parents.resize(depth + 1);
        parents[depth] = node;
        if (depth > 0) os << "    n" << parents[depth - 1] << " -> n" << node << ";\n";
        ++node;
    }
    os << "}\n";
}
