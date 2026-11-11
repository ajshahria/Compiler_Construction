#include "codegen.h"
#include "assembly.h"
#include <map>
#include <set>
#include <cstdio>
#include <sstream>

namespace {

class CodeGen {
public:
    AsmWriter w;
    std::map<std::string, Type> cells;        // every variable / temporary and its type
    std::vector<std::pair<std::string, double> > floatConsts;   // pool entries .LFn
    std::vector<std::pair<std::string, long> > intConsts;       // pool entries .LIn (for fild)
    std::vector<std::pair<std::string, std::string> > strings;   // pool entries .LSn
    int printLabels;

    CodeGen() : printLabels(0) {}

    // Label of a text constant in the data section (added to the pool on first use).
    std::string strLabel(const std::string& text) {
        for (size_t i = 0; i < strings.size(); i++)
            if (strings[i].second == text) return strings[i].first;
        std::ostringstream n; n << ".LS" << strings.size();
        strings.push_back(std::make_pair(n.str(), text));
        return n.str();
    }

    // Escape a string for an .asciz directive.
    static std::string asmEscape(const std::string& s) {
        std::string r;
        for (size_t i = 0; i < s.size(); i++) {
            unsigned char c = (unsigned char)s[i];
            if (c == '\n') r += "\\n";
            else if (c == '\t') r += "\\t";
            else if (c == '\r') r += "\\r";
            else if (c == '"') r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else if (c < 32) { char b[8]; sprintf(b, "\\%03o", c); r += b; }
            else r += (char)c;
        }
        return r;
    }

    static std::string cell(const std::string& n) { return "_v_" + n; }

    void collect(const TacProgram& p) {
        for (size_t i = 0; i < p.vars.size(); i++) cells[p.vars[i].uname] = p.vars[i].type;
        for (size_t i = 0; i < p.code.size(); i++) {
            const Instr& in = p.code[i];
            if (in.result.isName()) cells[in.result.name] = in.result.ty;
        }
    }

    std::string floatConst(double v) {
        std::ostringstream n; n << ".LF" << floatConsts.size();
        floatConsts.push_back(std::make_pair(n.str(), v));
        return n.str();
    }
    std::string intConst(long v) {
        std::ostringstream n; n << ".LI" << intConsts.size();
        intConsts.push_back(std::make_pair(n.str(), v));
        return n.str();
    }

    // Memory operand holding a float value (for flds).
    std::string fmem(const Operand& o) {
        if (o.kind == Operand::FloatC) return floatConst(o.fv);
        return cell(o.name);
    }
    // Operand for integer instructions: $imm or memory.
    std::string imem(const Operand& o) {
        if (o.kind == Operand::IntC || o.kind == Operand::BoolC) {
            std::ostringstream s; s << "$" << o.iv; return s.str();
        }
        return cell(o.name);
    }

    void loadInt(const Operand& o, const char* reg) { w.instr("movl", imem(o), reg); }

    void emitFloat(const Instr& in) {
        if (in.op == Op::Assign) {
            w.instr("flds", fmem(in.a));
            w.instr("fstps", cell(in.result.name));
        } else if (in.op == Op::Neg) {
            w.instr("flds", fmem(in.a));
            w.instr("fchs");
            w.instr("fstps", cell(in.result.name));
        } else if (in.op >= Op::Add && in.op <= Op::Div) {
            w.instr("flds", fmem(in.a));
            const char* m = in.op == Op::Add ? "fadds" : in.op == Op::Sub ? "fsubs"
                          : in.op == Op::Mul ? "fmuls" : "fdivs";
            w.instr(m, fmem(in.b));
            w.instr("fstps", cell(in.result.name));
        } else {   // comparison: compare a with b, result is a 0/1 int
            w.instr("flds", fmem(in.b));
            w.instr("flds", fmem(in.a));
            w.instr("fcomip", "%st(1)", "%st");
            w.instr("fstp", "%st(0)");
            const char* s = in.op == Op::Lt ? "setb" : in.op == Op::Le ? "setbe"
                          : in.op == Op::Gt ? "seta" : in.op == Op::Ge ? "setae"
                          : in.op == Op::Eq ? "sete" : "setne";
            w.instr(s, "%al");
            w.instr("movzbl", "%al", "%eax");
            w.instr("movl", "%eax", cell(in.result.name));
        }
    }

    void emitInt(const Instr& in) {
        std::string res = cell(in.result.name);
        switch (in.op) {
            case Op::Assign:
                loadInt(in.a, "%eax");
                w.instr("movl", "%eax", res);
                break;
            case Op::Neg:
                loadInt(in.a, "%eax"); w.instr("negl", "%eax"); w.instr("movl", "%eax", res);
                break;
            case Op::Not:
                loadInt(in.a, "%eax"); w.instr("xorl", "$1", "%eax"); w.instr("movl", "%eax", res);
                break;
            case Op::Add: case Op::Sub: case Op::Mul: case Op::And: case Op::Or: {
                const char* m = in.op == Op::Add ? "addl" : in.op == Op::Sub ? "subl"
                              : in.op == Op::Mul ? "imull" : in.op == Op::And ? "andl" : "orl";
                loadInt(in.a, "%eax");
                loadInt(in.b, "%ecx");
                w.instr(m, "%ecx", "%eax");
                w.instr("movl", "%eax", res);
                break;
            }
            case Op::Div: case Op::Mod:
                loadInt(in.a, "%eax");
                loadInt(in.b, "%ecx");
                w.instr("cltd");
                w.instr("idivl", "%ecx");
                w.instr("movl", in.op == Op::Div ? "%eax" : "%edx", res);
                break;
            default: {   // integer / bool comparison
                const char* s = in.op == Op::Lt ? "setl" : in.op == Op::Le ? "setle"
                              : in.op == Op::Gt ? "setg" : in.op == Op::Ge ? "setge"
                              : in.op == Op::Eq ? "sete" : "setne";
                loadInt(in.a, "%eax");
                loadInt(in.b, "%ecx");
                w.instr("cmpl", "%ecx", "%eax");
                w.instr(s, "%al");
                w.instr("movzbl", "%al", "%eax");
                w.instr("movl", "%eax", res);
            }
        }
    }

    void emitInstr(const Instr& in) {
        w.comment(instrStr(in));
        switch (in.op) {
            case Op::Label: w.label("." + in.label); return;
            case Op::Goto:  w.instr("jmp", "." + in.label); return;
            case Op::IfFalse:
                loadInt(in.a, "%eax");
                w.instr("cmpl", "$0", "%eax");
                w.instr("je", "." + in.label);
                return;
            case Op::IntToFloat:
                if (in.a.kind == Operand::IntC) w.instr("fildl", intConst(in.a.iv));
                else w.instr("fildl", cell(in.a.name));
                w.instr("fstps", cell(in.result.name));
                return;
            case Op::Print:  emitPrint(in, true);  return;    // print x;   (value + newline)
            case Op::Out:    emitPrint(in, false); return;    // cout << x;  (no newline)
            case Op::OutStr:
                w.instr("pushl", "$" + strLabel(in.label));
                w.instr("pushl", "$.Lfmt_str_n");
                w.instr("call", "_printf");
                w.instr("addl", "$8", "%esp");
                return;
            case Op::OutNl:
                w.instr("pushl", "$.Lstr_nl");
                w.instr("pushl", "$.Lfmt_str_n");
                w.instr("call", "_printf");
                w.instr("addl", "$8", "%esp");
                return;
            case Op::Read:   // cin >> x  ->  scanf("%d" or "%f", &x)
                w.instr("pushl", "$" + cell(in.result.name));
                w.instr("pushl", in.ty == TY_FLOAT ? "$.Lscan_float" : "$.Lscan_int");
                w.instr("call", "_scanf");
                w.instr("addl", "$8", "%esp");
                return;
            default: break;
        }
        // Operations on floats produce a float result, except comparisons (bool result).
        if (in.ty == TY_FLOAT) emitFloat(in);
        else emitInt(in);
    }

    // 'print' shows bool as true/false and ends the line; 'cout' shows bool as 1/0 and adds no newline.
    void emitPrint(const Instr& in, bool newline) {
        const char* fmtInt   = newline ? "$.Lfmt_int"   : "$.Lfmt_int_n";
        const char* fmtFloat = newline ? "$.Lfmt_float" : "$.Lfmt_float_n";
        const char* fmtStr   = "$.Lfmt_str";
        if (in.ty == TY_FLOAT) {
            w.instr("subl", "$12", "%esp");
            w.instr("movl", fmtFloat, "(%esp)");
            w.instr("flds", fmem(in.a));
            w.instr("fstpl", "4(%esp)");          // printf takes a double
            w.instr("call", "_printf");
            w.instr("addl", "$12", "%esp");
        } else if (in.ty == TY_BOOL && newline) {
            std::ostringstream n; n << ".Lpb" << ++printLabels;
            std::string lf = n.str() + "f", le = n.str() + "e";
            loadInt(in.a, "%eax");
            w.instr("cmpl", "$0", "%eax");
            w.instr("movl", "$.Lstr_true", "%ecx");
            w.instr("jne", le);
            w.instr("movl", "$.Lstr_false", "%ecx");
            w.label(le);
            w.instr("pushl", "%ecx");
            w.instr("pushl", fmtStr);
            w.instr("call", "_printf");
            w.instr("addl", "$8", "%esp");
            (void)lf;
        } else {
            w.instr("pushl", imem(in.a));
            w.instr("pushl", fmtInt);
            w.instr("call", "_printf");
            w.instr("addl", "$8", "%esp");
        }
    }

    std::string run(const TacProgram& p) {
        collect(p);
        // Code goes into 'w' first (it fills the constant pools); the data section is added in front.
        w.label("_main");
        w.instr("pushl", "%ebp");
        w.instr("movl", "%esp", "%ebp");
        w.instr("andl", "$-16", "%esp");
        w.instr("call", "___main");   // MinGW runtime initialisation
        for (size_t i = 0; i < p.code.size(); i++) emitInstr(p.code[i]);
        w.instr("xorl", "%eax", "%eax");
        w.instr("movl", "%ebp", "%esp");
        w.instr("popl", "%ebp");
        w.instr("ret");
        std::string text = w.str();

        AsmWriter out;
        out.comment("Generated by the Tricora Compiler - C++ version (x86, AT&T syntax, MinGW)");
        out.section(".data");
        out.label(".Lfmt_int");   out.directive(".asciz \"%d\\n\"");
        out.label(".Lfmt_float"); out.directive(".asciz \"%g\\n\"");
        out.label(".Lfmt_str");   out.directive(".asciz \"%s\\n\"");
        out.label(".Lstr_true");  out.directive(".asciz \"true\"");
        out.label(".Lstr_false"); out.directive(".asciz \"false\"");
        out.label(".Lfmt_int_n");   out.directive(".asciz \"%d\"");
        out.label(".Lfmt_float_n"); out.directive(".asciz \"%g\"");
        out.label(".Lfmt_str_n");   out.directive(".asciz \"%s\"");
        out.label(".Lstr_nl");      out.directive(".asciz \"\\n\"");
        out.label(".Lscan_int");    out.directive(".asciz \"%d\"");
        out.label(".Lscan_float");  out.directive(".asciz \"%f\"");
        for (size_t i = 0; i < strings.size(); i++) {      // text from cout << "..."
            out.label(strings[i].first);
            out.directive(".asciz \"" + asmEscape(strings[i].second) + "\"");
        }
        for (size_t i = 0; i < floatConsts.size(); i++) {
            std::ostringstream v; v.precision(17); v << floatConsts[i].second;
            out.label(floatConsts[i].first); out.directive(".float " + v.str());
        }
        for (size_t i = 0; i < intConsts.size(); i++) {
            std::ostringstream v; v << intConsts[i].second;
            out.label(intConsts[i].first); out.directive(".long " + v.str());
        }
        for (std::map<std::string, Type>::iterator it = cells.begin(); it != cells.end(); ++it) {
            out.label(cell(it->first));
            out.directive(it->second == TY_FLOAT ? ".float 0.0" : ".long 0");
        }
        out.section(".text");
        out.global("_main");
        return out.str() + text;
    }
};

}  // namespace

std::string generateAssembly(const TacProgram& p) {
    CodeGen g;
    return g.run(p);
}
