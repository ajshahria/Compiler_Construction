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
    Binary, Unary, Ternary, IntLit, FloatLit, BoolLit, Ident,
    Index, Call, Function      // bonus: a[i], f(x, y), int f(int x) { ... }
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

// A variable, or (bonus) an array: 'int a[5];' 'int b[] = {1, 2};' (length > 0,
// initial values in initList; length -1 marks a bad size, reported by semantic analysis).
struct VarDeclNode : Node {
    Type declType;
    std::string name;
    std::string uname;  // unique TAC name (set by semantic analysis)
    Node* init;         // may be null
    long length;        // arrays: number of elements; 0 for a variable
    std::vector<Node*> initList;
    VarDeclNode(Type t, const std::string& n, Node* i, int l)
        : Node(NodeKind::VarDecl, l), declType(t), name(n), init(i), length(0) {}
    void print(std::ostream& os, int indent) const;
};

// x = e, or (bonus) a[i] = e when index is set.
struct AssignNode : Node {
    std::string name, uname;
    Node* value;
    Node* index;        // may be null
    AssignNode(const std::string& n, Node* v, int l, Node* idx = NULL)
        : Node(NodeKind::Assign, l), name(n), value(v), index(idx) {}
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

// a[i]  (bonus)
struct IndexNode : Node {
    std::string name, uname;
    Node* index;
    IndexNode(const std::string& n, Node* i, int l) : Node(NodeKind::Index, l), name(n), index(i) {}
    void print(std::ostream& os, int indent) const;
};

// f(a, b)  (bonus) - an expression, or a statement on its own: 'f(a);'
struct CallNode : Node {
    std::string name;
    std::vector<Node*> args;
    CallNode(const std::string& n, int l) : Node(NodeKind::Call, l), name(n) {}
    void print(std::ostream& os, int indent) const;
};

struct Param {
    Type type;
    std::string name, uname;
    int line;
};

// int f(int a, float b) { ... }  (bonus)
struct FunctionNode : Node {
    Type retType;       // TY_VOID for a void function
    std::string name;
    std::vector<Param> params;
    BlockNode* body;
    FunctionNode(Type t, const std::string& n, BlockNode* b, int l)
        : Node(NodeKind::Function, l), retType(t), name(n), body(b) {}
    void print(std::ostream& os, int indent) const;
};

// 'cin >> a >> b[i];'  Needs #include <iostream>. Each target is an Ident or (bonus) an Index.
struct CinNode : Node {
    std::vector<Node*> vars;
    CinNode(int l) : Node(NodeKind::Cin, l) {}
    void print(std::ostream& os, int indent) const;
};

// The AST as a Graphviz digraph (the --dot option, bonus).
void printDot(const Node* root, std::ostream& os);

#endif
