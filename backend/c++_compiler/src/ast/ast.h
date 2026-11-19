// ast.h - Abstract Syntax Tree node definitions (phase 2 output).
// Parentheses and semicolons do not appear: only meaningful constructs.
#ifndef AST_H
#define AST_H

#include <iostream>
#include <string>
#include <vector>
#include "types.h"

enum class NodeKind {
    Program, Block, DeclGroup, VarDecl, Assign, If, While, DoWhile, Break, Continue,
    Return, Switch, Print, Cout, Cin,
    Binary, Unary, Ternary, IntLit, FloatLit, BoolLit, Ident
};

struct Node {
    NodeKind kind;
    int line;
    Type type;          // value type of an expression; filled by semantic analysis
    Node(NodeKind k, int l) : kind(k), line(l), type(TY_ERROR) {}
    virtual ~Node() {}
    virtual void print(std::ostream& os, int indent) const = 0;
};

// Program and Block hold a list of statements. A Program is the global scope.
// A DeclGroup ('int a, b;') and a switch-case body are statement lists WITHOUT their own scope.
struct BlockNode : Node {
    std::vector<Node*> stmts;
    BlockNode(NodeKind k, int l) : Node(k, l) {}
    void print(std::ostream& os, int indent) const;
};

struct VarDeclNode : Node {
    Type declType;
    std::string name;
    std::string uname;  // unique TAC name (set by semantic analysis)
    Node* init;         // may be null
    VarDeclNode(Type t, const std::string& n, Node* i, int l)
        : Node(NodeKind::VarDecl, l), declType(t), name(n), init(i) {}
    void print(std::ostream& os, int indent) const;
};

struct AssignNode : Node {
    std::string name, uname;
    Node* value;
    AssignNode(const std::string& n, Node* v, int l)
        : Node(NodeKind::Assign, l), name(n), value(v) {}
    void print(std::ostream& os, int indent) const;
};

struct IfNode : Node {
    Node *cond, *thenStmt, *elseStmt;  // elseStmt may be null
    IfNode(Node* c, Node* t, Node* e, int l)
        : Node(NodeKind::If, l), cond(c), thenStmt(t), elseStmt(e) {}
    void print(std::ostream& os, int indent) const;
};

// while loop. 'step' is only used by 'for' loops (runs after the body, and is where 'continue' jumps).
struct WhileNode : Node {
    Node *cond, *body, *step;
    WhileNode(Node* c, Node* b, int l, Node* s = NULL)
        : Node(NodeKind::While, l), cond(c), body(b), step(s) {}
    void print(std::ostream& os, int indent) const;
};

struct DoWhileNode : Node {
    Node *body, *cond;
    DoWhileNode(Node* b, Node* c, int l) : Node(NodeKind::DoWhile, l), body(b), cond(c) {}
    void print(std::ostream& os, int indent) const;
};

// 'break;' and 'continue;'
struct JumpNode : Node {
    JumpNode(NodeKind k, int l) : Node(k, l) {}
    void print(std::ostream& os, int indent) const;
};

struct ReturnNode : Node {
    Node* value;        // may be null
    ReturnNode(Node* v, int l) : Node(NodeKind::Return, l), value(v) {}
    void print(std::ostream& os, int indent) const;
};

struct SwitchCase {
    bool isDefault;
    long value;
    int line;
    BlockNode* body;
};

struct SwitchNode : Node {
    Node* expr;
    std::vector<SwitchCase> cases;
    SwitchNode(Node* e, int l) : Node(NodeKind::Switch, l), expr(e) {}
    void print(std::ostream& os, int indent) const;
};

// The manual's 'print e;' (prints the value and a newline)
struct PrintNode : Node {
    Node* expr;
    PrintNode(Node* e, int l) : Node(NodeKind::Print, l), expr(e) {}
    void print(std::ostream& os, int indent) const;
};

// 'cout << a << "text" << endl;'  One item per '<<'. Needs #include <iostream>.
struct CoutItem {
    int kind;           // 0 = expression, 1 = text, 2 = endl, 3 = (cond ? "text" : "text")
    Node* expr;         // the value (kind 0) or the condition (kind 3)
    std::string text;   // the text (kind 1) or the "true" text (kind 3)
    std::string text2;  // the "false" text (kind 3)
};

struct CoutNode : Node {
    std::vector<CoutItem> items;
    CoutNode(int l) : Node(NodeKind::Cout, l) {}
    void print(std::ostream& os, int indent) const;
};

struct BinaryNode : Node {
    std::string op;
    Node *left, *right;
    BinaryNode(const std::string& o, Node* a, Node* b, int l)
        : Node(NodeKind::Binary, l), op(o), left(a), right(b) {}
    void print(std::ostream& os, int indent) const;
};

struct UnaryNode : Node {
    std::string op;     // "!" or "-"
    Node* operand;
    UnaryNode(const std::string& o, Node* e, int l)
        : Node(NodeKind::Unary, l), op(o), operand(e) {}
    void print(std::ostream& os, int indent) const;
};

// cond ? a : b
struct TernaryNode : Node {
    Node *cond, *a, *b;
    TernaryNode(Node* c, Node* x, Node* y, int l) : Node(NodeKind::Ternary, l), cond(c), a(x), b(y) {}
    void print(std::ostream& os, int indent) const;
};

struct IntLitNode : Node {
    long value;
    IntLitNode(long v, int l) : Node(NodeKind::IntLit, l), value(v) {}
    void print(std::ostream& os, int indent) const;
};

struct FloatLitNode : Node {
    double value;
    FloatLitNode(double v, int l) : Node(NodeKind::FloatLit, l), value(v) {}
    void print(std::ostream& os, int indent) const;
};

struct BoolLitNode : Node {
    bool value;
    BoolLitNode(bool v, int l) : Node(NodeKind::BoolLit, l), value(v) {}
    void print(std::ostream& os, int indent) const;
};

struct IdentNode : Node {
    std::string name, uname;
    IdentNode(const std::string& n, int l) : Node(NodeKind::Ident, l), name(n) {}
    void print(std::ostream& os, int indent) const;
};

// 'cin >> a >> b;'  Needs #include <iostream>.
struct CinNode : Node {
    std::vector<IdentNode*> vars;
    CinNode(int l) : Node(NodeKind::Cin, l) {}
    void print(std::ostream& os, int indent) const;
};

#endif
