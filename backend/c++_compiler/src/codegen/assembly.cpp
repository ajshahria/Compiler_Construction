#include "assembly.h"

void AsmWriter::comment(const std::string& t)   { out_ << "    # " << t << "\n"; }
void AsmWriter::section(const std::string& n)   { out_ << "    " << n << "\n"; }
void AsmWriter::global(const std::string& s)    { out_ << "    .globl " << s << "\n"; }
void AsmWriter::label(const std::string& n)     { out_ << n << ":\n"; }
void AsmWriter::instr(const std::string& op)    { out_ << "    " << op << "\n"; }
void AsmWriter::instr(const std::string& op, const std::string& a) {
    out_ << "    " << op << " " << a << "\n";
}
void AsmWriter::instr(const std::string& op, const std::string& a, const std::string& b) {
    out_ << "    " << op << " " << a << ", " << b << "\n";
}
void AsmWriter::directive(const std::string& t) { out_ << "    " << t << "\n"; }
