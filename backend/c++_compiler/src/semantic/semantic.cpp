// semantic.cpp - rules the grammar cannot express:
//   undeclared variable, redeclaration, scope violation, type mismatch,
//   invalid assignment, invalid operands, non-bool conditions,
//   break/continue outside loops, switch rules, cout/cin without <iostream>.
// int -> float widening is allowed; every other mixing is an error.
#include "semantic.h"
#include "diagnostics.h"
#include "headers.h"
#include <set>
#include <sstream>

static SymbolTable* table;
static int errors;
static int loopDepth;      // how many loops we are inside (for 'continue')
static int switchDepth;    // how many switches we are inside (for 'break')

static void err(int line, const std::string& msg) {
    errors++;
    reportError(PH_SEMANTIC, line, msg);
}

static bool canAssign(Type to, Type from) {
    if (to == from) return true;
    return to == TY_FLOAT && from == TY_INT;   // widening only
}

// Find a variable; report "scope violation" or "undeclared" if it is not visible.
static Symbol* resolve(const std::string& name, int line) {
    Symbol* s = table->lookup(name);
    if (s) return s;
    if (Symbol* c = table->findClosed(name)) {
        std::ostringstream m;
        m << "scope violation: '" << name << "' was declared in an inner block (line "
          << c->line << ") and is not visible here";
        err(line, m.str());
    } else {
        err(line, "undeclared variable '" + name + "'");
    }
    return NULL;
}

static Type checkExpr(Node* n);
static void checkStmt(Node* n);
static void checkCondition(Node* cond, const char* what);

static Type checkBinary(BinaryNode* b) {
    Type l = checkExpr(b->left);
    Type r = checkExpr(b->right);
    if (l == TY_ERROR || r == TY_ERROR) return TY_ERROR;   // already reported, avoid cascades
    const std::string& op = b->op;

    if (op == "+" || op == "-" || op == "*" || op == "/") {
        if (!isNumeric(l) || !isNumeric(r)) {
            err(b->line, "invalid operands to '" + op + "': " + typeName(l) + " and " +
                typeName(r) + " (arithmetic needs int or float)");
            return TY_ERROR;
        }
        return (l == TY_FLOAT || r == TY_FLOAT) ? TY_FLOAT : TY_INT;
    }
    if (op == "%") {
        if (l != TY_INT || r != TY_INT) {
            err(b->line, std::string("invalid operands to '%': ") + typeName(l) + " and " +
                typeName(r) + " ('%' needs two int operands)");
            return TY_ERROR;
        }
        return TY_INT;
    }
    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
        if (!isNumeric(l) || !isNumeric(r)) {
            err(b->line, "invalid operands to '" + op + "': " + typeName(l) + " and " +
                typeName(r) + " (relational needs int or float)");
            return TY_ERROR;
        }
        return TY_BOOL;
    }
    if (op == "==" || op == "!=") {
        bool ok = (isNumeric(l) && isNumeric(r)) || (l == TY_BOOL && r == TY_BOOL);
        if (!ok) {
            err(b->line, "invalid operands to '" + op + "': cannot compare " + typeName(l) +
                " with " + typeName(r));
            return TY_ERROR;
        }
        return TY_BOOL;
    }
    // && and ||
    if (l != TY_BOOL || r != TY_BOOL) {
        err(b->line, "invalid operands to '" + op + "': " + typeName(l) + " and " +
            typeName(r) + " (logical operators need bool operands)");
        return TY_ERROR;
    }
    return TY_BOOL;
}

static Type checkExpr(Node* n) {
    Type t = TY_ERROR;
    switch (n->kind) {
        case NodeKind::IntLit:   t = TY_INT;   break;
        case NodeKind::FloatLit: t = TY_FLOAT; break;
        case NodeKind::BoolLit:  t = TY_BOOL;  break;
        case NodeKind::Ident: {
            IdentNode* id = static_cast<IdentNode*>(n);
            Symbol* s = resolve(id->name, id->line);
            if (s) { id->uname = s->uname; t = s->type; }
            break;
        }
        case NodeKind::Unary: {
            UnaryNode* u = static_cast<UnaryNode*>(n);
            Type o = checkExpr(u->operand);
            if (o == TY_ERROR) break;
            if (u->op == "!") {
                if (o != TY_BOOL) err(u->line, std::string("invalid operand to '!': ") + typeName(o) + " (needs bool)");
                else t = TY_BOOL;
            } else {
                if (!isNumeric(o)) err(u->line, std::string("invalid operand to unary '-': ") + typeName(o) + " (needs int or float)");
                else t = o;
            }
            break;
        }
        case NodeKind::Binary:
            t = checkBinary(static_cast<BinaryNode*>(n));
            break;
        case NodeKind::Ternary: {
            TernaryNode* q = static_cast<TernaryNode*>(n);
            checkCondition(q->cond, "?:");
            Type a = checkExpr(q->a), b = checkExpr(q->b);
            if (a == TY_ERROR || b == TY_ERROR) break;
            if (isNumeric(a) && isNumeric(b)) t = (a == TY_FLOAT || b == TY_FLOAT) ? TY_FLOAT : TY_INT;
            else if (a == TY_BOOL && b == TY_BOOL) t = TY_BOOL;
            else err(q->line, std::string("invalid operands to '?:': ") + typeName(a) + " and " + typeName(b) +
                     " (both branches must have the same kind of type)");
            break;
        }
        default:
            break;
    }
    n->type = t;
    return t;
}

static void checkCondition(Node* cond, const char* what) {
    Type t = checkExpr(cond);
    if (t != TY_ERROR && t != TY_BOOL)
        err(cond->line, std::string("condition of '") + what + "' must be bool, found " + typeName(t));
}

static void checkList(BlockNode* b) {
    for (size_t i = 0; i < b->stmts.size(); i++) checkStmt(b->stmts[i]);
}

static void checkIostream(int line, const char* what) {
    if (!hasHeader("iostream"))
        err(line, std::string("'") + what + "' is not declared: add #include <iostream>");
}

static void checkStmt(Node* n) {
    if (!n) return;
    switch (n->kind) {
        case NodeKind::Block:
            table->enterScope();
            checkList(static_cast<BlockNode*>(n));
            table->exitScope();
            break;
        case NodeKind::DeclGroup:       // 'int a, b;' - no scope of its own
            checkList(static_cast<BlockNode*>(n));
            break;
        case NodeKind::VarDecl: {
            VarDeclNode* d = static_cast<VarDeclNode*>(n);
            // Check the initializer first: 'int x = x;' must not see the new x.
            Type it = d->init ? checkExpr(d->init) : TY_ERROR;
            Symbol *existing = NULL, *created = NULL;
            if (!table->insert(d->name, d->declType, d->line, &existing, &created)) {
                std::ostringstream m;
                m << "redeclaration of '" << d->name << "' in the same scope (first declared at line "
                  << existing->line << ")";
                err(d->line, m.str());
            } else {
                d->uname = created->uname;
            }
            if (d->init && it != TY_ERROR && !canAssign(d->declType, it))
                err(d->line, std::string("type mismatch: cannot initialize ") + typeName(d->declType) +
                    " '" + d->name + "' with a value of type " + typeName(it));
            break;
        }
        case NodeKind::Assign: {
            AssignNode* a = static_cast<AssignNode*>(n);
            Type vt = checkExpr(a->value);
            Symbol* s = resolve(a->name, a->line);
            if (!s) break;
            a->uname = s->uname;
            a->type = s->type;
            if (vt != TY_ERROR && !canAssign(s->type, vt))
                err(a->line, std::string("invalid assignment: cannot assign ") + typeName(vt) +
                    " to " + typeName(s->type) + " variable '" + a->name + "'");
            break;
        }
        case NodeKind::If: {
            IfNode* f = static_cast<IfNode*>(n);
            checkCondition(f->cond, "if");
            checkStmt(f->thenStmt);
            checkStmt(f->elseStmt);
            break;
        }
        case NodeKind::While: {
            WhileNode* w = static_cast<WhileNode*>(n);
            checkCondition(w->cond, "while");
            loopDepth++;
            checkStmt(w->body);
            loopDepth--;
            checkStmt(w->step);
            break;
        }
        case NodeKind::DoWhile: {
            DoWhileNode* d = static_cast<DoWhileNode*>(n);
            loopDepth++;
            checkStmt(d->body);
            loopDepth--;
            checkCondition(d->cond, "do-while");
            break;
        }
        case NodeKind::Break:
            if (loopDepth == 0 && switchDepth == 0)
                err(n->line, "'break' statement not within a loop or switch");
            break;
        case NodeKind::Continue:
            if (loopDepth == 0)
                err(n->line, "'continue' statement not within a loop");
            break;
        case NodeKind::Return: {
            ReturnNode* r = static_cast<ReturnNode*>(n);
            if (r->value) {
                Type t = checkExpr(r->value);
                if (t != TY_ERROR && t != TY_INT && t != TY_BOOL)
                    err(r->line, std::string("return value must be int, found ") + typeName(t));
            }
            break;
        }
        case NodeKind::Switch: {
            SwitchNode* s = static_cast<SwitchNode*>(n);
            Type t = checkExpr(s->expr);
            if (t != TY_ERROR && t != TY_INT)
                err(s->line, std::string("switch expression must be int, found ") + typeName(t));
            table->enterScope();            // the whole switch body is one scope
            switchDepth++;
            std::set<long> seen;
            bool hasDefault = false;
            for (size_t i = 0; i < s->cases.size(); i++) {
                SwitchCase& c = s->cases[i];
                if (c.isDefault) {
                    if (hasDefault) err(c.line, "duplicate 'default' label in switch");
                    hasDefault = true;
                } else if (!seen.insert(c.value).second) {
                    std::ostringstream m;
                    m << "duplicate case value " << c.value << " in switch";
                    err(c.line, m.str());
                }
                checkList(c.body);
            }
            switchDepth--;
            table->exitScope();
            break;
        }
        case NodeKind::Print:
            checkExpr(static_cast<PrintNode*>(n)->expr);
            break;
        case NodeKind::Cout: {
            CoutNode* c = static_cast<CoutNode*>(n);
            checkIostream(c->line, "cout");
            for (size_t i = 0; i < c->items.size(); i++) {
                if (c->items[i].kind == 0) checkExpr(c->items[i].expr);
                else if (c->items[i].kind == 3) checkCondition(c->items[i].expr, "?:");
            }
            break;
        }
        case NodeKind::Cin: {
            CinNode* c = static_cast<CinNode*>(n);
            checkIostream(c->line, "cin");
            for (size_t i = 0; i < c->vars.size(); i++) {
                IdentNode* v = c->vars[i];
                Symbol* s = resolve(v->name, v->line);
                if (s) { v->uname = s->uname; v->type = s->type; }
            }
            break;
        }
        default:
            break;
    }
}

int analyze(BlockNode* program, SymbolTable& symtab) {
    table = &symtab;
    errors = 0;
    loopDepth = 0;
    switchDepth = 0;
    checkList(program);   // the program itself is the global scope (already open)
    return errors;
}
