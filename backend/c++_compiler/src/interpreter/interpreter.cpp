// interpreter.cpp - executes the TAC directly.
//
// Every variable and temporary is a cell; an array (bonus) is a list of cells.
// A function (bonus) is a 'func f: ... endfunc' block that only runs through a
// call: execution that reaches 'func' jumps past its 'endfunc'. A call saves the
// callee's locals (its variables, arrays and temporaries), so a recursive call
// gets fresh ones, and restores them when it returns.
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

// One active call.
struct Frame {
    size_t returnPc;
    Operand result;                                   // where the returned value goes (may be None)
    Type type;                                        // the function's return type
    std::map<std::string, Value> savedCells;          // the caller's copy of the callee's locals
    std::map<std::string, std::vector<Value> > savedArrays;
    std::vector<Value> args;
    size_t nextArg;
};

const size_t MAX_CALLS = 10000;
}

bool interpret(const TacProgram& p, std::ostream& os, std::istream& is) {
    std::map<std::string, Value> mem;
    std::map<std::string, std::vector<Value> > arrays;
    std::map<std::string, Type> functionType;
    std::map<std::string, std::vector<std::string> > localCells, localArrays;   // per function
    for (size_t i = 0; i < p.vars.size(); i++) {      // every variable starts at 0
        const Symbol& s = p.vars[i];
        if (s.isFunction) { functionType[s.name] = s.type; continue; }
        Value v; v.ty = s.type;
        if (s.length > 0) {
            arrays[s.uname] = std::vector<Value>((size_t)s.length, v);
            if (!s.function.empty()) localArrays[s.function].push_back(s.uname);
        } else {
            mem[s.uname] = v;
            if (!s.function.empty()) localCells[s.function].push_back(s.uname);
        }
    }
    std::map<std::string, size_t> labels, functions;
    std::vector<size_t> skip(p.code.size(), 0);       // func: the index of its endfunc
    std::string inside;
    for (size_t i = 0; i < p.code.size(); i++) {
        const Instr& in = p.code[i];
        if (in.op == Op::Label) labels[in.label] = i;
        if (in.op == Op::Func) { functions[in.label] = i; inside = in.label; }
        if (in.op == Op::EndFunc) {
            skip[functions[inside]] = i;
            inside.clear();
        }
        // temporaries made inside a function are its locals too
        if (!inside.empty() && in.result.kind == Operand::Temp)
            localCells[inside].push_back(in.result.name);
    }

    struct Get {
        std::map<std::string, Value>& m;
        Value operator()(const Operand& o) {
            return o.isName() ? m[o.name] : constant(o);
        }
    } get = { mem };

    std::vector<Frame> frames;
    std::vector<Value> pending;                       // arguments passed with param

    // a[i]: the element, or NULL after reporting an index out of bounds.
    struct Element {
        std::map<std::string, std::vector<Value> >& arrays;
        Value* operator()(const std::string& name, long index) {
            std::vector<Value>& a = arrays[name];
            if (index < 0 || index >= (long)a.size()) {
                std::cerr << "Runtime error: index " << index << " is out of bounds for array '" << name
                          << "' of size " << a.size() << "\n";
                return NULL;
            }
            return &a[(size_t)index];
        }
    } element = { arrays };

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
            case Op::Read:
            case Op::ReadElem: {  // cin >> x  /  cin >> a[i]
                Value* target = &mem[in.result.name];
                if (in.op == Op::ReadElem && !(target = element(in.result.name, get(in.a).iv))) return false;
                Value v = *target;
                v.ty = in.result.ty;
                if (v.ty == TY_FLOAT) { double d = 0; if (is >> d) v.fv = d; }
                else { long x = 0; if (is >> x) v.iv = (v.ty == TY_BOOL) ? (x != 0) : x; }
                *target = v;
                break;
            }
            case Op::Load: {
                Value* e = element(in.a.name, get(in.b).iv);
                if (!e) return false;
                mem[in.result.name] = *e;
                break;
            }
            case Op::Store: {
                Value* e = element(in.result.name, get(in.a).iv);
                if (!e) return false;
                Value v = get(in.b); v.ty = in.result.ty;
                *e = v;
                break;
            }
            case Op::Func: pc = skip[pc]; break;     // reached without a call: skip the function
            case Op::Param: pending.push_back(get(in.a)); break;
            case Op::Call: {
                if (!functions.count(in.label)) { std::cerr << "Runtime error: unknown function '" << in.label << "'\n"; return false; }
                if (frames.size() == MAX_CALLS) { std::cerr << "Runtime error: too many nested calls (endless recursion?)\n"; return false; }
                Frame f;
                f.returnPc = pc;
                f.result = in.result;
                f.type = functionType[in.label];
                f.args.assign(pending.end() - in.argc, pending.end());
                pending.resize(pending.size() - in.argc);
                f.nextArg = 0;
                const std::vector<std::string>& cells = localCells[in.label];
                const std::vector<std::string>& arrs = localArrays[in.label];
                for (size_t k = 0; k < cells.size(); k++) {   // save, then start at 0
                    f.savedCells[cells[k]] = mem[cells[k]];
                    Value zero; zero.ty = mem[cells[k]].ty; mem[cells[k]] = zero;
                }
                for (size_t k = 0; k < arrs.size(); k++) {
                    f.savedArrays[arrs[k]] = arrays[arrs[k]];
                    for (size_t e = 0; e < arrays[arrs[k]].size(); e++) {
                        Value zero; zero.ty = arrays[arrs[k]][e].ty; arrays[arrs[k]][e] = zero;
                    }
                }
                frames.push_back(f);
                pc = functions[in.label];               // continues after 'func f:'
                break;
            }
            case Op::Formal: {
                if (frames.empty() || frames.back().nextArg >= frames.back().args.size()) {
                    std::cerr << "Runtime error: a function was called with too few arguments\n";
                    return false;
                }
                Value v = frames.back().args[frames.back().nextArg++];
                v.ty = in.result.ty;
                mem[in.result.name] = v;
                break;
            }
            case Op::Return:
            case Op::EndFunc: {
                if (frames.empty()) { std::cerr << "Runtime error: return outside of a function call\n"; return false; }
                Value v;
                if (in.op == Op::Return && in.a.kind != Operand::None) v = get(in.a);
                Frame f = frames.back();
                frames.pop_back();
                for (std::map<std::string, Value>::iterator it = f.savedCells.begin(); it != f.savedCells.end(); ++it)
                    mem[it->first] = it->second;        // the caller's locals come back
                for (std::map<std::string, std::vector<Value> >::iterator it = f.savedArrays.begin();
                     it != f.savedArrays.end(); ++it)
                    arrays[it->first] = it->second;
                pc = f.returnPc;
                if (f.result.kind != Operand::None) { v.ty = f.type; mem[f.result.name] = v; }
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
