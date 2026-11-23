#include "imports/imports.h"

#include <cstdio>
#include <iomanip>
#include <vector>

extern int lexicalErrors;   // lexer.l: the lexical error count

namespace {

struct Package { const char* prefix; const char* purpose; };

// Standard packages, most specific first, with what they provide.
const Package PACKAGES[] = {
    {"java.util.concurrent", "threads, executors, locks"},
    {"java.util.function", "functional interfaces (lambdas)"},
    {"java.util.stream", "streams over collections"},
    {"java.util.regex", "regular expressions"},
    {"java.util", "collections, Scanner, Random"},
    {"java.lang", "core classes (always imported)"},
    {"java.io", "input and output streams"},
    {"java.nio", "buffers, channels, files"},
    {"java.math", "BigInteger, BigDecimal"},
    {"java.text", "number and date formatting"},
    {"java.time", "dates, times, durations"},
    {"java.net", "networking, URLs"},
    {"java.sql", "database access (JDBC)"},
    {"java.awt", "windowing and graphics"},
    {"javax.swing", "graphical user interfaces"},
    {"java", "Java standard library"},
    {"javax", "Java extension library"},
};

struct Import { std::string name; bool isStatic; int line; const Package* package; };
std::vector<Import> imports;

// The package an imported name belongs to, or nullptr if it is not standard.
const Package* findPackage(const std::string& name) {
    for (const Package& p : PACKAGES) {
        std::string prefix = p.prefix;
        if (name.compare(0, prefix.size(), prefix) == 0 &&
            name.size() > prefix.size() && name[prefix.size()] == '.')
            return &p;
    }
    return nullptr;
}

}  // namespace

void addImport(const std::string& name, bool isStatic, int line) {
    const Package* package = findPackage(name);
    if (!package) {
        std::fprintf(stderr, "line %d: lexical error: unknown package '%s' "
                             "(only java.* and javax.* standard imports are supported)\n",
                     line, name.c_str());
        ++lexicalErrors;
        return;
    }
    for (const Import& i : imports)
        if (i.name == name && i.isStatic == isStatic) return;   // a repeated import is ignored
    imports.push_back(Import{name, isStatic, line, package});
}

bool isImported(const std::string& package, const std::string& name) {
    for (const Import& i : imports)
        if (!i.isStatic && (i.name == package + "." + name || i.name == package + ".*")) return true;
    return false;
}

void printImports(std::ostream& out) {
    if (imports.empty()) { out << "(no imports)\n"; return; }
    out << std::left << std::setw(30) << "Import" << std::setw(6) << "Line"
        << std::setw(22) << "Package" << "Role in this compiler\n";
    out << "------------------------------------------------------------------------------------\n";
    for (const Import& i : imports) {
        std::string shown = (i.isStatic ? "static " : "") + i.name;
        std::string role = std::string(i.package->prefix) == "java.lang"
                               ? "always available (no effect)" : "accepted (no effect)";
        out << std::left << std::setw(30) << shown << std::setw(6) << i.line
            << std::setw(22) << i.package->prefix << role << "  - " << i.package->purpose << '\n';
    }
}
