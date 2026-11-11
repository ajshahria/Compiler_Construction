#include "interpreter.h"
#include <iostream>
#include <map>

namespace {
struct Value {
    Type ty;
    long iv;       // int and bool (0/1)
    double fv;     // float
    Value() : ty(TY_INT), iv(0), fv(0) {}
};

Value constant(const Operand& o) {
    Value v; v.ty = o.ty; v.iv = o.iv; v.fv = o.fv;
    return v;
}
}

bool interpret(const TacProgram& p, std::ostream& os, std::istream& is) {
    std::map<std::string, Value> mem;
    for (size_t i = 0; i < p.vars.size(); i++) {      // every variable starts at 0
        Value v; v.ty = p.vars[i].type;
        mem[p.vars[i].uname] = v;
    }
    std::map<std::string, size_t> labels;
    for (size_t i = 0; i < p.code.size(); i++)
        if (p.code[i].op == Op::Label) labels[p.code[i].label] = i;

    struct Get {
        std::map<std::string, Value>& m;
        Value operator()(const Operand& o) {
            return o.isName() ? m[o.name] : constant(o);
        }
    } get = { mem };

    long steps = 0;
    for (size_t pc = 0; pc < p.code.size(); pc++) {
        if (++steps > 10000000) { std::cerr << "Runtime error: step limit exceeded (infinite loop?)\n"; return false; }
        const Instr& in = p.code[pc];
        switch (in.op) {
            case Op::Label: break;
            case Op::Goto: pc = labels[in.label]; break;
            case Op::IfFalse: if (!get(in.a).iv) pc = labels[in.label]; break;
            case Op::Print: {
                Value v = get(in.a);
                if (v.ty == TY_FLOAT) os << v.fv << "\n";
                else if (v.ty == TY_BOOL) os << (v.iv ? "true" : "false") << "\n";
                else os << v.iv << "\n";
                break;
            }
            case Op::Out: {   // cout << value (no newline; bool prints as 1/0 like C++)
                Value v = get(in.a);
                if (v.ty == TY_FLOAT) os << v.fv; else os << v.iv;
                break;
            }
            case Op::OutStr: os << in.label; break;
            case Op::OutNl:  os << "\n"; break;
            case Op::Read: {  // cin >> x
                Value v = mem[in.result.name];
                v.ty = in.result.ty;
                if (v.ty == TY_FLOAT) { double d = 0; if (is >> d) v.fv = d; }
                else { long x = 0; if (is >> x) v.iv = (v.ty == TY_BOOL) ? (x != 0) : x; }
                mem[in.result.name] = v;
                break;
            }
            case Op::Assign: { Value v = get(in.a); v.ty = in.result.ty; mem[in.result.name] = v; break; }
            case Op::IntToFloat: { Value v; v.ty = TY_FLOAT; v.fv = (double)get(in.a).iv; mem[in.result.name] = v; break; }
            case Op::Not: { Value v; v.ty = TY_BOOL; v.iv = !get(in.a).iv; mem[in.result.name] = v; break; }
            case Op::Neg: {
                Value a = get(in.a), v; v.ty = a.ty;
                if (a.ty == TY_FLOAT) v.fv = -a.fv; else v.iv = -a.iv;
                mem[in.result.name] = v; break;
            }
            default: {
                Value a = get(in.a), b = get(in.b), v;
                v.ty = in.result.ty;
                if (in.ty == TY_FLOAT) {
                    double x = a.fv, y = b.fv;
                    switch (in.op) {
                        case Op::Add: v.fv = x + y; break;
                        case Op::Sub: v.fv = x - y; break;
                        case Op::Mul: v.fv = x * y; break;
                        case Op::Div:
                            if (y == 0) { std::cerr << "Runtime error: division by zero\n"; return false; }
                            v.fv = x / y; break;
                        case Op::Lt: v.iv = x < y; break;
                        case Op::Gt: v.iv = x > y; break;
                        case Op::Le: v.iv = x <= y; break;
                        case Op::Ge: v.iv = x >= y; break;
                        case Op::Eq: v.iv = x == y; break;
                        case Op::Ne: v.iv = x != y; break;
                        default: break;
                    }
                } else {
                    long x = a.iv, y = b.iv;
                    switch (in.op) {
                        case Op::Add: v.iv = x + y; break;
                        case Op::Sub: v.iv = x - y; break;
                        case Op::Mul: v.iv = x * y; break;
                        case Op::Div:
                            if (y == 0) { std::cerr << "Runtime error: division by zero\n"; return false; }
                            v.iv = x / y; break;
                        case Op::Mod:
                            if (y == 0) { std::cerr << "Runtime error: modulo by zero\n"; return false; }
                            v.iv = x % y; break;
                        case Op::Lt: v.iv = x < y; break;
                        case Op::Gt: v.iv = x > y; break;
                        case Op::Le: v.iv = x <= y; break;
                        case Op::Ge: v.iv = x >= y; break;
                        case Op::Eq: v.iv = x == y; break;
                        case Op::Ne: v.iv = x != y; break;
                        case Op::And: v.iv = x && y; break;
                        case Op::Or:  v.iv = x || y; break;
                        default: break;
                    }
                }
                mem[in.result.name] = v;
            }
        }
    }
    return true;
}
