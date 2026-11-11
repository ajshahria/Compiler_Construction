// assembly.h - tiny helper that formats x86 (32-bit, AT&T syntax) assembly text.
#ifndef ASSEMBLY_H
#define ASSEMBLY_H

#include <sstream>
#include <string>

class AsmWriter {
public:
    void comment(const std::string& text);
    void section(const std::string& name);                 // .data, .text ...
    void global(const std::string& sym);
    void label(const std::string& name);
    void instr(const std::string& op);                     // no operands
    void instr(const std::string& op, const std::string& a);
    void instr(const std::string& op, const std::string& a, const std::string& b);
    void directive(const std::string& text);               // raw line such as ".asciz ..."
    std::string str() const { return out_.str(); }

private:
    std::ostringstream out_;
};

#endif
