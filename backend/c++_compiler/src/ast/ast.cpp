// ast.cpp - indented text printer for the AST.
#include "ast.h"

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
    os << "VarDecl " << typeName(declType) << " " << name << " (line " << line << ")\n";
    if (init) init->print(os, indent + 1);
}

void AssignNode::print(std::ostream& os, int indent) const {
    pad(os, indent);
    os << "Assign " << name << " (line " << line << ")\n";
    value->print(os, indent + 1);
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
