// semantic.cpp - rules the grammar cannot express:
//   undeclared variable, redeclaration, scope violation, type mismatch,
//   invalid assignment, invalid operands, non-bool conditions,
//   break/continue outside loops, switch rules, cout/cin without <iostream>.
// int -> float widening is allowed; every other mixing is an error.
//
// Bonus (manual section 14):
//   arrays    - a positive size and at most that many initial values; an array is only
//               used with an int index; a variable or function cannot be indexed
//   functions - defined at the top level; a call names a function with one argument of a
//               fitting type per parameter; a void function is not a value; return matches
//               the function's type; main cannot be called. Every function is known before
//               the program is checked, so functions can call each other (and themselves).
#include "semantic.h"
#include "diagnostics.h"
#include "headers.h"
#include <set>
#include <sstream>

static SymbolTable* table;
static int errors;
static int loopDepth;      // how many loops we are inside (for 'continue')
static int switchDepth;    // how many switches we are inside (for 'break')
static FunctionNode* currentFunction;   // the function being checked (NULL: main or top level)

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

// The index of a[i] must be an int.
static void checkIndex(Node* index, const std::string& array) {
    Type t = checkExpr(index);
    if (t != TY_ERROR && t != TY_INT)
        err(index->line, "array index of '" + array + "' must be int, found " + typeName(t));
}

// a[i]: returns the array's symbol, or NULL after reporting an error.
static Symbol* resolveArray(const std::string& name, int line) {
    Symbol* s = resolve(name, line);
    if (!s) return NULL;
    if (s->isFunction || s->length == 0) {
        err(line, "'" + name + "' is not an array and cannot be indexed");
        return NULL;
    }
    return s;
}

// f(a, b): one argument of a fitting type per parameter. Returns the return type
// (TY_VOID for a void function), or TY_ERROR.
static Type checkCall(CallNode* c) {
    std::vector<Type> args;
    for (size_t i = 0; i < c->args.size(); i++) args.push_back(checkExpr(c->args[i]));
    Symbol* s = table->lookup(c->name);
    if (!s) {
        err(c->line, "undeclared function '" + c->name + "'");
        return TY_ERROR;
    }
    if (!s->isFunction) {
        err(c->line, "'" + c->name + "' is not a function and cannot be called");
        return TY_ERROR;
    }
    if (args.size() != s->params.size()) {
        std::ostringstream m;
        m << "function '" << c->name << "' takes " << s->params.size() << " argument(s) but "
          << args.size() << " were given";
        err(c->line, m.str());
        return s->type;
    }
    for (size_t i = 0; i < args.size(); i++)
        if (args[i] != TY_ERROR && !canAssign(s->params[i], args[i])) {
            std::ostringstream m;
            m << "argument " << i + 1 << " of '" << c->name << "' must be " << typeName(s->params[i])
              << ", found " << typeName(args[i]);
            err(c->line, m.str());
        }
    return s->type;
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
            if (!s) break;
            if (s->isFunction)
                err(id->line, "function '" + id->name + "' used without a call: write " + id->name + "(...)");
            else if (s->length > 0)
                err(id->line, "array '" + id->name + "' used without an index: write " + id->name + "[i]");
            else { id->uname = s->uname; t = s->type; }
            break;
        }
        case NodeKind::Index: {
            IndexNode* x = static_cast<IndexNode*>(n);
            Symbol* s = resolveArray(x->name, x->line);
            checkIndex(x->index, x->name);
            if (s) { x->uname = s->uname; t = s->type; }
            break;
        }
        case NodeKind::Call: {
            CallNode* c = static_cast<CallNode*>(n);
            t = checkCall(c);
            if (t == TY_VOID) {
                err(c->line, "void function '" + c->name + "' does not return a value");
                t = TY_ERROR;
            }
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

// Declares a variable or array in the current scope (reports a redeclaration).
static Symbol* declare(const std::string& name, Type type, long length, int line) {
    Symbol extra;
    extra.length = length > 0 ? length : 0;
    if (currentFunction) extra.function = currentFunction->name;   // saved around recursive calls
    Symbol *existing = NULL, *created = NULL;
    if (!table->insert(name, type, line, &existing, &created, extra)) {
        std::ostringstream m;
        m << "redeclaration of '" << name << "' in the same scope (first declared at line "
          << existing->line << ")";
        err(line, m.str());
        return NULL;
    }
    return created;
}

static void checkFunction(FunctionNode* f) {
    if (currentFunction || table->lookup(f->name) == NULL) {   // inside a block or another function
        err(f->line, "function '" + f->name + "' must be defined at the top level");
        return;
    }
    int outerLoops = loopDepth, outerSwitches = switchDepth;
    loopDepth = switchDepth = 0;
    currentFunction = f;
    table->enterScope();       // the parameters and the body share one scope
    for (size_t i = 0; i < f->params.size(); i++) {
        Symbol* p = declare(f->params[i].name, f->params[i].type, 0, f->params[i].line);
        if (p) f->params[i].uname = p->uname;
    }
    checkList(f->body);
    table->exitScope();
    currentFunction = NULL;
    loopDepth = outerLoops;
    switchDepth = outerSwitches;
}

static void checkReturn(ReturnNode* r) {
    Type t = r->value ? checkExpr(r->value) : TY_VOID;
    if (!currentFunction) {     // main: an int (or bool) status
        if (t != TY_ERROR && t != TY_VOID && t != TY_INT && t != TY_BOOL)
            err(r->line, std::string("return value must be int, found ") + typeName(t));
        return;
    }
    const std::string& name = currentFunction->name;
    Type want = currentFunction->retType;
    if (want == TY_VOID && t != TY_VOID)
        err(r->line, "void function '" + name + "' cannot return a value");
    else if (want != TY_VOID && t == TY_VOID)
        err(r->line, "function '" + name + "' must return a value of type " + typeName(want));
    else if (t != TY_ERROR && t != TY_VOID && !canAssign(want, t))
        err(r->line, "function '" + name + "' returns " + typeName(want) + ", found " + typeName(t));
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
            if (d->length != 0) {       // an array
                for (size_t i = 0; i < d->initList.size(); i++) {
                    Type it = checkExpr(d->initList[i]);
                    if (it != TY_ERROR && !canAssign(d->declType, it)) {
                        std::ostringstream m;
                        m << "type mismatch: value " << i + 1 << " of array '" << d->name << "' is "
                          << typeName(it) << ", expected " << typeName(d->declType);
                        err(d->line, m.str());
                    }
                }
                if (d->length < 0)
                    err(d->line, "array '" + d->name + "' must have a positive size");
                else if ((long)d->initList.size() > d->length) {
                    std::ostringstream m;
                    m << "too many values for array '" << d->name << "' (size " << d->length << ", "
                      << d->initList.size() << " values)";
                    err(d->line, m.str());
                }
                if (Symbol* s = declare(d->name, d->declType, d->length > 0 ? d->length : 1, d->line))
                    d->uname = s->uname;
                break;
            }
            // Check the initializer first: 'int x = x;' must not see the new x.
            Type it = d->init ? checkExpr(d->init) : TY_ERROR;
            if (Symbol* s = declare(d->name, d->declType, 0, d->line)) d->uname = s->uname;
            if (d->init && it != TY_ERROR && !canAssign(d->declType, it))
                err(d->line, std::string("type mismatch: cannot initialize ") + typeName(d->declType) +
                    " '" + d->name + "' with a value of type " + typeName(it));
            break;
        }
        case NodeKind::Assign: {
            AssignNode* a = static_cast<AssignNode*>(n);
            Type vt = checkExpr(a->value);
            if (a->index) {             // a[i] = e
                Symbol* s = resolveArray(a->name, a->line);
                checkIndex(a->index, a->name);
                if (!s) break;
                a->uname = s->uname;
                a->type = s->type;
                if (vt != TY_ERROR && !canAssign(s->type, vt))
                    err(a->line, std::string("invalid assignment: cannot assign ") + typeName(vt) +
                        " to an element of " + typeName(s->type) + " array '" + a->name + "'");
                break;
            }
            Symbol* s = resolve(a->name, a->line);
            if (!s) break;
            if (s->isFunction || s->length > 0) {
                err(a->line, std::string("invalid assignment: cannot assign to the ") +
                    (s->isFunction ? "function '" : "whole array '") + a->name + "'");
                break;
            }
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
        case NodeKind::Return:
            checkReturn(static_cast<ReturnNode*>(n));
            break;
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
                if (c->vars[i]->kind == NodeKind::Index) {     // cin >> a[i]
                    checkExpr(c->vars[i]);
                    continue;
                }
                IdentNode* v = static_cast<IdentNode*>(c->vars[i]);
                Symbol* s = resolve(v->name, v->line);
                if (s && (s->isFunction || s->length > 0))
                    err(v->line, "cin cannot read into " + std::string(s->isFunction ? "function '" : "array '") +
                        v->name + "'" + (s->isFunction ? "" : ": read one element, " + v->name + "[i]"));
                else if (s) { v->uname = s->uname; v->type = s->type; }
            }
            break;
        }
        case NodeKind::Call:            // f(x); - a void function may be called as a statement
            checkCall(static_cast<CallNode*>(n));
            break;
        case NodeKind::Function:
            checkFunction(static_cast<FunctionNode*>(n));
            break;
        default:
            break;
    }
}

// Every function at the top level is entered in the global scope first.
static void declareFunctions(BlockNode* program) {
    for (size_t i = 0; i < program->stmts.size(); i++) {
        Node* s = program->stmts[i];
        if (!s || s->kind != NodeKind::Function) continue;
        FunctionNode* f = static_cast<FunctionNode*>(s);
        if (f->name == "main") {
            err(f->line, "'main' must be written as 'int main()'");
            continue;
        }
        Symbol extra;
        extra.isFunction = true;
        for (size_t k = 0; k < f->params.size(); k++) extra.params.push_back(f->params[k].type);
        Symbol *existing = NULL, *created = NULL;
        if (!table->insert(f->name, f->retType, f->line, &existing, &created, extra)) {
            std::ostringstream m;
            m << "redefinition of '" << f->name << "' (first declared at line " << existing->line << ")";
            err(f->line, m.str());
        }
    }
}

int analyze(BlockNode* program, SymbolTable& symtab) {
    table = &symtab;
    errors = 0;
    loopDepth = 0;
    switchDepth = 0;
    currentFunction = NULL;
    declareFunctions(program);
    checkList(program);   // the program itself is the global scope (already open)
    return errors;
}
