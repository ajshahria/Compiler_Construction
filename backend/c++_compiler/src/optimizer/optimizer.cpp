#include "optimizer.h"
#include <map>
#include <set>

static bool isBinaryOp(Op op) {
    return op >= Op::Add && op <= Op::Or;
}

static double numVal(const Operand& o) { return o.kind == Operand::FloatC ? o.fv : (double)o.iv; }

// Try to compute 'op' on constant operands. Returns false if it cannot (e.g. division by zero).
static bool fold(const Instr& in, Operand& out) {
    if (in.op == Op::IntToFloat) { out = Operand::floatc((double)in.a.iv); return true; }
    if (in.op == Op::Not) { out = Operand::boolc(!in.a.iv); return true; }
    if (in.op == Op::Neg) {
        out = in.a.kind == Operand::FloatC ? Operand::floatc(-in.a.fv) : Operand::intc(-in.a.iv);
        return true;
    }
    if (!isBinaryOp(in.op)) return false;
    const Operand &a = in.a, &b = in.b;
    if (a.ty == TY_BOOL) {
        switch (in.op) {
            case Op::And: out = Operand::boolc(a.iv && b.iv); return true;
            case Op::Or:  out = Operand::boolc(a.iv || b.iv); return true;
            case Op::Eq:  out = Operand::boolc(a.iv == b.iv); return true;
            case Op::Ne:  out = Operand::boolc(a.iv != b.iv); return true;
            default: return false;
        }
    }
    if (a.ty == TY_FLOAT) {
        double x = numVal(a), y = numVal(b);
        switch (in.op) {
            case Op::Add: out = Operand::floatc(x + y); return true;
            case Op::Sub: out = Operand::floatc(x - y); return true;
            case Op::Mul: out = Operand::floatc(x * y); return true;
            case Op::Div: if (y == 0) return false; out = Operand::floatc(x / y); return true;
            case Op::Lt:  out = Operand::boolc(x < y);  return true;
            case Op::Gt:  out = Operand::boolc(x > y);  return true;
            case Op::Le:  out = Operand::boolc(x <= y); return true;
            case Op::Ge:  out = Operand::boolc(x >= y); return true;
            case Op::Eq:  out = Operand::boolc(x == y); return true;
            case Op::Ne:  out = Operand::boolc(x != y); return true;
            default: return false;
        }
    }
    long x = a.iv, y = b.iv;
    switch (in.op) {
        case Op::Add: out = Operand::intc(x + y); return true;
        case Op::Sub: out = Operand::intc(x - y); return true;
        case Op::Mul: out = Operand::intc(x * y); return true;
        case Op::Div: if (y == 0) return false; out = Operand::intc(x / y); return true;
        case Op::Mod: if (y == 0) return false; out = Operand::intc(x % y); return true;
        case Op::Lt:  out = Operand::boolc(x < y);  return true;
        case Op::Gt:  out = Operand::boolc(x > y);  return true;
        case Op::Le:  out = Operand::boolc(x <= y); return true;
        case Op::Ge:  out = Operand::boolc(x >= y); return true;
        case Op::Eq:  out = Operand::boolc(x == y); return true;
        case Op::Ne:  out = Operand::boolc(x != y); return true;
        default: return false;
    }
}

static bool definesResult(Op op) {
    return op == Op::Assign || op == Op::Not || op == Op::Neg || op == Op::IntToFloat ||
           op == Op::Read || isBinaryOp(op);
}

// Pass 1: constant propagation + folding + branch simplification, per basic block.
static std::vector<Instr> foldPass(const std::vector<Instr>& code) {
    std::vector<Instr> out;
    std::map<std::string, Operand> known;   // name -> constant value
    for (size_t k = 0; k < code.size(); k++) {
        Instr in = code[k];
        if (in.op == Op::Label) { known.clear(); out.push_back(in); continue; }   // join point

        // substitute known constants into the operands
        if (in.a.isName() && known.count(in.a.name)) in.a = known[in.a.name];
        if (in.b.isName() && known.count(in.b.name)) in.b = known[in.b.name];

        if (in.op == Op::IfFalse && in.a.isConst()) {
            if (!in.a.iv) { Instr g; g.op = Op::Goto; g.label = in.label; out.push_back(g); }
            continue;   // always-true condition: the branch is never taken, drop it
        }
        if (in.op == Op::Goto) { out.push_back(in); continue; }

        if (definesResult(in.op)) {
            bool aConst = in.a.isConst(), bConst = in.b.isConst() || in.b.kind == Operand::None;
            Operand folded;
            if (in.op != Op::Assign && aConst && bConst && fold(in, folded)) {
                in.op = Op::Assign; in.a = folded; in.b = Operand();
                in.ty = in.result.ty;
            }
            if (in.op == Op::Assign && in.a.isConst()) known[in.result.name] = in.a;
            else known.erase(in.result.name);
        }
        out.push_back(in);
    }
    return out;
}

// Pass 2: drop code after an unconditional goto until the next label.
static std::vector<Instr> unreachablePass(const std::vector<Instr>& code) {
    std::vector<Instr> out;
    bool dead = false;
    for (size_t k = 0; k < code.size(); k++) {
        if (code[k].op == Op::Label) dead = false;
        if (dead) continue;
        out.push_back(code[k]);
        if (code[k].op == Op::Goto) dead = true;
    }
    return out;
}

static void countUses(const std::vector<Instr>& code, std::map<std::string, int>& uses) {
    for (size_t k = 0; k < code.size(); k++) {
        if (code[k].a.isName()) uses[code[k].a.name]++;
        if (code[k].b.isName()) uses[code[k].b.name]++;
    }
}

// Pass 3: remove temporaries that are never used, and collapse "t = a op b; x = t".
static std::vector<Instr> deadTempPass(const std::vector<Instr>& code) {
    std::map<std::string, int> uses;
    countUses(code, uses);
    std::vector<Instr> out;
    for (size_t k = 0; k < code.size(); k++) {
        const Instr& in = code[k];
        if (definesResult(in.op) && in.result.kind == Operand::Temp && uses[in.result.name] == 0)
            continue;   // dead temp
        // collapse: previous instr defines temp t used only here, this is x = t
        if (in.op == Op::Assign && in.a.kind == Operand::Temp && uses[in.a.name] == 1 &&
            !out.empty() && definesResult(out.back().op) &&
            out.back().result.kind == Operand::Temp && out.back().result.name == in.a.name) {
            out.back().result = in.result;
            continue;
        }
        out.push_back(in);
    }
    return out;
}

// Pass 4: "goto L; L:" -> "L:", and unused labels removed.
static std::vector<Instr> jumpPass(const std::vector<Instr>& code) {
    std::vector<Instr> out;
    for (size_t k = 0; k < code.size(); k++) {
        if (code[k].op == Op::Goto && k + 1 < code.size() && code[k + 1].op == Op::Label &&
            code[k + 1].label == code[k].label)
            continue;
        out.push_back(code[k]);
    }
    std::set<std::string> targets;
    for (size_t k = 0; k < out.size(); k++)
        if (out[k].op == Op::Goto || out[k].op == Op::IfFalse) targets.insert(out[k].label);
    std::vector<Instr> res;
    for (size_t k = 0; k < out.size(); k++)
        if (out[k].op == Op::Label && !targets.count(out[k].label)) continue;
        else res.push_back(out[k]);
    return res;
}

static bool sameCode(const std::vector<Instr>& a, const std::vector<Instr>& b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); k++)
        if (instrStr(a[k]) != instrStr(b[k])) return false;
    return true;
}

TacProgram optimize(const TacProgram& in) {
    TacProgram p = in;
    for (int round = 0; round < 20; round++) {   // repeat until nothing changes
        std::vector<Instr> c = p.code;
        c = foldPass(c);
        c = unreachablePass(c);
        c = deadTempPass(c);
        c = jumpPass(c);
        bool same = sameCode(c, p.code);
        p.code = c;
        if (same) break;
    }
    return p;
}
