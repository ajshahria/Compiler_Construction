// tac.h - Phase 4: Three Address Code (intermediate representation).
// Each instruction has one operator and at most two operands plus a result.
#ifndef TAC_H
#define TAC_H

#include <ostream>
#include <string>
#include <vector>
#include "ast.h"
#include "symbol_table.h"

enum class Op {
    Assign,        // result = a
    Add, Sub, Mul, Div, Mod,
    Lt, Gt, Le, Ge, Eq, Ne,
    And, Or,       // bool operands
    Not, Neg,      // unary
    IntToFloat,    // result = (float) a
    Label,         // label:
    Goto,          // goto label
    IfFalse,       // ifFalse a goto label
    Print,         // print a        (the manual's print: value + newline)
    Out,           // out a          (cout << a, no newline)
    OutStr,        // outs "text"    (cout << "text"; text kept in 'label')
    OutNl,         // outnl          (cout << endl)
    Read           // read x         (cin >> x)
};

struct Operand {
    enum Kind { None, Var, Temp, IntC, FloatC, BoolC } kind;
    std::string name;      // Var / Temp name
    long iv;               // IntC value, or 0/1 for BoolC
    double fv;             // FloatC value
    Type ty;
    Operand() : kind(None), iv(0), fv(0), ty(TY_INT) {}
    bool isConst() const { return kind == IntC || kind == FloatC || kind == BoolC; }
    bool isName() const { return kind == Var || kind == Temp; }
    static Operand var(const std::string& n, Type t);
    static Operand temp(const std::string& n, Type t);
    static Operand intc(long v);
    static Operand floatc(double v);
    static Operand boolc(bool v);
};

struct Instr {
    Op op;
    Operand result, a, b;
    Type ty;               // type the operation works on (operand type after conversion)
    std::string label;     // Label / Goto / IfFalse target
    Instr() : op(Op::Assign), ty(TY_INT) {}
};

struct TacProgram {
    std::vector<Instr> code;
    std::vector<Symbol> vars;   // declared variables (for the data section)
};

TacProgram generateTAC(BlockNode* program, const SymbolTable& table);

std::string operandStr(const Operand& o);
std::string instrStr(const Instr& i);
void printTAC(const TacProgram& p, std::ostream& os);

#endif
