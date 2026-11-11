// tac.cpp - walks the (already type-checked) AST and emits Three Address Code.
// Expressions return an Operand; each operator creates a fresh temporary t1, t2, ...
// Control flow uses labels L1, L2, ... with ifFalse/goto.
// break/continue/return/switch are translated into plain gotos to labels.
#include "tac.h"
#include <iomanip>
#include <sstream>

Operand Operand::var(const std::string& n, Type t) { Operand o; o.kind = Var; o.name = n; o.ty = t; return o; }
Operand Operand::temp(const std::string& n, Type t) { Operand o; o.kind = Temp; o.name = n; o.ty = t; return o; }
Operand Operand::intc(long v) { Operand o; o.kind = IntC; o.iv = v; o.ty = TY_INT; return o; }
Operand Operand::floatc(double v) { Operand o; o.kind = FloatC; o.fv = v; o.ty = TY_FLOAT; return o; }
Operand Operand::boolc(bool v) { Operand o; o.kind = BoolC; o.iv = v ? 1 : 0; o.ty = TY_BOOL; return o; }

namespace {

// Where 'break' and 'continue' jump to inside the loop / switch we are generating.
struct Ctx {
    std::string brk, cont;   // cont is empty for a switch
    bool contUsed;
};

class Generator {
public:
    TacProgram prog;
    int tempCount, labelCount;
    std::vector<Ctx> ctx;
    std::string exitLabel;   // target of 'return', created on first use
    Generator() : tempCount(0), labelCount(0) {}

    Operand newTemp(Type t) {
        std::ostringstream ss; ss << "t" << ++tempCount;
        return Operand::temp(ss.str(), t);
    }
    std::string newLabel() {
        std::ostringstream ss; ss << "L" << ++labelCount;
        return ss.str();
    }
    void emit(const Instr& i) { prog.code.push_back(i); }

    void emitLabel(const std::string& l) { Instr i; i.op = Op::Label; i.label = l; emit(i); }
    void emitGoto(const std::string& l) { Instr i; i.op = Op::Goto; i.label = l; emit(i); }
    void emitIfFalse(const Operand& c, const std::string& l) {
        Instr i; i.op = Op::IfFalse; i.a = c; i.label = l; i.ty = TY_BOOL; emit(i);
    }
    void emitAssign(const Operand& dst, const Operand& src) {
        Instr i; i.op = Op::Assign; i.result = dst; i.a = src; i.ty = dst.ty; emit(i);
    }

    // Convert an int operand to float (constants are converted directly).
    Operand toFloat(const Operand& o) {
        if (o.ty == TY_FLOAT) return o;
        if (o.kind == Operand::IntC) return Operand::floatc((double)o.iv);
        Operand t = newTemp(TY_FLOAT);
        Instr i; i.op = Op::IntToFloat; i.result = t; i.a = o; i.ty = TY_FLOAT; emit(i);
        return t;
    }

    Operand expr(Node* n) {
        switch (n->kind) {
            case NodeKind::IntLit:   return Operand::intc(static_cast<IntLitNode*>(n)->value);
            case NodeKind::FloatLit: return Operand::floatc(static_cast<FloatLitNode*>(n)->value);
            case NodeKind::BoolLit:  return Operand::boolc(static_cast<BoolLitNode*>(n)->value);
            case NodeKind::Ident: {
                IdentNode* id = static_cast<IdentNode*>(n);
                return Operand::var(id->uname, id->type);
            }
            case NodeKind::Unary: {
                UnaryNode* u = static_cast<UnaryNode*>(n);
                Operand a = expr(u->operand);
                Operand t = newTemp(u->type);
                Instr i; i.op = (u->op == "!") ? Op::Not : Op::Neg;
                i.result = t; i.a = a; i.ty = a.ty;
                emit(i);
                return t;
            }
            case NodeKind::Binary: return binary(static_cast<BinaryNode*>(n));
            case NodeKind::Ternary: {
                // t = cond ? a : b   ==>   ifFalse cond goto Le; t = a; goto Lend; Le: t = b; Lend:
                TernaryNode* q = static_cast<TernaryNode*>(n);
                Operand c = expr(q->cond);
                Operand res = newTemp(q->type);
                std::string lElse = newLabel(), lEnd = newLabel();
                emitIfFalse(c, lElse);
                Operand a = expr(q->a);
                if (q->type == TY_FLOAT) a = toFloat(a);
                emitAssign(res, a);
                emitGoto(lEnd);
                emitLabel(lElse);
                Operand b = expr(q->b);
                if (q->type == TY_FLOAT) b = toFloat(b);
                emitAssign(res, b);
                emitLabel(lEnd);
                return res;
            }
            default: return Operand();
        }
    }

    Operand binary(BinaryNode* b) {
        Operand l = expr(b->left);
        Operand r = expr(b->right);
        Type opTy = l.ty;
        // mixed int/float arithmetic or comparison: promote the int side
        if (l.ty != r.ty && isNumeric(l.ty) && isNumeric(r.ty)) {
            l = toFloat(l);
            r = toFloat(r);
            opTy = TY_FLOAT;
        }
        Instr i;
        const std::string& o = b->op;
        if (o == "+") i.op = Op::Add;       else if (o == "-") i.op = Op::Sub;
        else if (o == "*") i.op = Op::Mul;  else if (o == "/") i.op = Op::Div;
        else if (o == "%") i.op = Op::Mod;  else if (o == "<") i.op = Op::Lt;
        else if (o == ">") i.op = Op::Gt;   else if (o == "<=") i.op = Op::Le;
        else if (o == ">=") i.op = Op::Ge;  else if (o == "==") i.op = Op::Eq;
        else if (o == "!=") i.op = Op::Ne;  else if (o == "&&") i.op = Op::And;
        else i.op = Op::Or;
        Operand t = newTemp(b->type);
        i.result = t; i.a = l; i.b = r; i.ty = opTy;
        emit(i);
        return t;
    }

    void list(const std::vector<Node*>& stmts) {
        for (size_t i = 0; i < stmts.size(); i++) stmt(stmts[i]);
    }

    void stmt(Node* n) {
        if (!n) return;
        switch (n->kind) {
            case NodeKind::Program:
            case NodeKind::Block:
            case NodeKind::DeclGroup:
                list(static_cast<BlockNode*>(n)->stmts);
                break;
            case NodeKind::VarDecl: {
                VarDeclNode* d = static_cast<VarDeclNode*>(n);
                if (d->init) {
                    Operand v = expr(d->init);
                    if (d->declType == TY_FLOAT) v = toFloat(v);
                    emitAssign(Operand::var(d->uname, d->declType), v);
                }
                break;
            }
            case NodeKind::Assign: {
                AssignNode* a = static_cast<AssignNode*>(n);
                Operand v = expr(a->value);
                if (a->type == TY_FLOAT) v = toFloat(v);
                emitAssign(Operand::var(a->uname, a->type), v);
                break;
            }
            case NodeKind::If: {
                IfNode* f = static_cast<IfNode*>(n);
                Operand c = expr(f->cond);
                std::string lElse = newLabel();
                emitIfFalse(c, lElse);
                stmt(f->thenStmt);
                if (f->elseStmt) {
                    std::string lEnd = newLabel();
                    emitGoto(lEnd);
                    emitLabel(lElse);
                    stmt(f->elseStmt);
                    emitLabel(lEnd);
                } else {
                    emitLabel(lElse);
                }
                break;
            }
            case NodeKind::While: {
                // L1: cond; ifFalse -> L2; body; [Lc: step]; goto L1; L2:
                WhileNode* w = static_cast<WhileNode*>(n);
                std::string lStart = newLabel(), lEnd = newLabel();
                std::string lCont = w->step ? newLabel() : lStart;   // 'continue' target
                emitLabel(lStart);
                Operand c = expr(w->cond);
                emitIfFalse(c, lEnd);
                Ctx cx; cx.brk = lEnd; cx.cont = lCont; cx.contUsed = false;
                ctx.push_back(cx);
                stmt(w->body);
                if (w->step) {
                    if (ctx.back().contUsed) emitLabel(lCont);
                    stmt(w->step);
                }
                ctx.pop_back();
                emitGoto(lStart);
                emitLabel(lEnd);
                break;
            }
            case NodeKind::DoWhile: {
                // L1: body; [Lc:] cond; ifFalse -> L2; goto L1; L2:
                DoWhileNode* d = static_cast<DoWhileNode*>(n);
                std::string lStart = newLabel(), lEnd = newLabel(), lCont = newLabel();
                emitLabel(lStart);
                Ctx cx; cx.brk = lEnd; cx.cont = lCont; cx.contUsed = false;
                ctx.push_back(cx);
                stmt(d->body);
                if (ctx.back().contUsed) emitLabel(lCont);
                ctx.pop_back();
                Operand c = expr(d->cond);
                emitIfFalse(c, lEnd);
                emitGoto(lStart);
                emitLabel(lEnd);
                break;
            }
            case NodeKind::Break:
                emitGoto(ctx.back().brk);
                break;
            case NodeKind::Continue: {
                for (int k = (int)ctx.size() - 1; k >= 0; k--) {
                    if (ctx[k].cont.empty()) continue;      // skip switches
                    ctx[k].contUsed = true;
                    emitGoto(ctx[k].cont);
                    break;
                }
                break;
            }
            case NodeKind::Return: {
                ReturnNode* r = static_cast<ReturnNode*>(n);
                if (r->value) expr(r->value);
                if (exitLabel.empty()) exitLabel = newLabel();
                emitGoto(exitLabel);
                break;
            }
            case NodeKind::Switch: {
                // dispatch: for each case, "t = v != c; ifFalse t goto Lcase"; then goto default/end.
                SwitchNode* s = static_cast<SwitchNode*>(n);
                Operand v = expr(s->expr);
                std::string lEnd = newLabel();
                std::vector<std::string> labels;
                std::string lDefault = lEnd;
                for (size_t i = 0; i < s->cases.size(); i++) {
                    labels.push_back(newLabel());
                    if (s->cases[i].isDefault) lDefault = labels[i];
                }
                for (size_t i = 0; i < s->cases.size(); i++) {
                    if (s->cases[i].isDefault) continue;
                    Operand t = newTemp(TY_BOOL);
                    Instr in; in.op = Op::Ne; in.result = t; in.a = v;
                    in.b = Operand::intc(s->cases[i].value); in.ty = TY_INT;
                    emit(in);
                    emitIfFalse(t, labels[i]);
                }
                emitGoto(lDefault);
                Ctx cx; cx.brk = lEnd; cx.contUsed = false;   // no 'cont': continue skips this switch
                ctx.push_back(cx);
                for (size_t i = 0; i < s->cases.size(); i++) {
                    emitLabel(labels[i]);
                    stmt(s->cases[i].body);
                }
                ctx.pop_back();
                emitLabel(lEnd);
                break;
            }
            case NodeKind::Print: {
                Operand v = expr(static_cast<PrintNode*>(n)->expr);
                Instr i; i.op = Op::Print; i.a = v; i.ty = v.ty;
                emit(i);
                break;
            }
            case NodeKind::Cout: {
                CoutNode* c = static_cast<CoutNode*>(n);
                for (size_t k = 0; k < c->items.size(); k++) {
                    Instr i;
                    if (c->items[k].kind == 0) {
                        Operand v = expr(c->items[k].expr);
                        i.op = Op::Out; i.a = v; i.ty = v.ty;
                    } else if (c->items[k].kind == 1) {
                        i.op = Op::OutStr; i.label = c->items[k].text;
                    } else if (c->items[k].kind == 3) {
                        // (c ? "a" : "b")  ==>  ifFalse c goto Le; outs "a"; goto Lend; Le: outs "b"; Lend:
                        Operand cond = expr(c->items[k].expr);
                        std::string lElse = newLabel(), lEnd = newLabel();
                        emitIfFalse(cond, lElse);
                        i.op = Op::OutStr; i.label = c->items[k].text;
                        emit(i);
                        emitGoto(lEnd);
                        emitLabel(lElse);
                        i.label = c->items[k].text2;
                        emit(i);
                        emitLabel(lEnd);
                        continue;
                    } else {
                        i.op = Op::OutNl;
                    }
                    emit(i);
                }
                break;
            }
            case NodeKind::Cin: {
                CinNode* c = static_cast<CinNode*>(n);
                for (size_t k = 0; k < c->vars.size(); k++) {
                    Instr i; i.op = Op::Read;
                    i.result = Operand::var(c->vars[k]->uname, c->vars[k]->type);
                    i.ty = c->vars[k]->type;
                    emit(i);
                }
                break;
            }
            default:
                break;
        }
    }
};

}  // namespace

TacProgram generateTAC(BlockNode* program, const SymbolTable& table) {
    Generator g;
    g.stmt(program);
    if (!g.exitLabel.empty()) g.emitLabel(g.exitLabel);   // 'return' jumps here
    g.prog.vars = table.allSymbols();
    return g.prog;
}

std::string operandStr(const Operand& o) {
    std::ostringstream ss;
    switch (o.kind) {
        case Operand::Var:
        case Operand::Temp:   ss << o.name; break;
        case Operand::IntC:   ss << o.iv; break;
        case Operand::FloatC: {
            std::ostringstream f; f << o.fv;
            std::string s = f.str();
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) s += ".0";
            ss << s;
            break;
        }
        case Operand::BoolC:  ss << (o.iv ? "true" : "false"); break;
        default: break;
    }
    return ss.str();
}

static const char* opSym(Op op) {
    switch (op) {
        case Op::Add: return "+";  case Op::Sub: return "-";
        case Op::Mul: return "*";  case Op::Div: return "/";
        case Op::Mod: return "%";  case Op::Lt:  return "<";
        case Op::Gt:  return ">";  case Op::Le:  return "<=";
        case Op::Ge:  return ">="; case Op::Eq:  return "==";
        case Op::Ne:  return "!="; case Op::And: return "&&";
        case Op::Or:  return "||";
        default: return "?";
    }
}

// Show a string with escapes so it prints on one line: "a\nb".
static std::string quoted(const std::string& s) {
    std::string r = "\"";
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '\n') r += "\\n";
        else if (c == '\t') r += "\\t";
        else if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r + "\"";
}

std::string instrStr(const Instr& i) {
    std::ostringstream ss;
    switch (i.op) {
        case Op::Assign:     ss << operandStr(i.result) << " = " << operandStr(i.a); break;
        case Op::Not:        ss << operandStr(i.result) << " = !" << operandStr(i.a); break;
        case Op::Neg:        ss << operandStr(i.result) << " = -" << operandStr(i.a); break;
        case Op::IntToFloat: ss << operandStr(i.result) << " = (float) " << operandStr(i.a); break;
        case Op::Label:      ss << i.label << ":"; break;
        case Op::Goto:       ss << "goto " << i.label; break;
        case Op::IfFalse:    ss << "ifFalse " << operandStr(i.a) << " goto " << i.label; break;
        case Op::Print:      ss << "print " << operandStr(i.a); break;
        case Op::Out:        ss << "out " << operandStr(i.a); break;
        case Op::OutStr:     ss << "outs " << quoted(i.label); break;
        case Op::OutNl:      ss << "outnl"; break;
        case Op::Read:       ss << "read " << operandStr(i.result); break;
        default:             ss << operandStr(i.result) << " = " << operandStr(i.a) << " "
                                << opSym(i.op) << " " << operandStr(i.b); break;
    }
    return ss.str();
}

void printTAC(const TacProgram& p, std::ostream& os) {
    for (size_t k = 0; k < p.code.size(); k++) {
        if (p.code[k].op == Op::Label) os << instrStr(p.code[k]) << "\n";
        else os << "    " << instrStr(p.code[k]) << "\n";
    }
}
