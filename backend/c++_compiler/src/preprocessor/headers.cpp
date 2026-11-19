#include "headers.h"
#include "diagnostics.h"
#include <iomanip>
#include <vector>

namespace {

struct Known { const char* name; const char* category; const char* purpose; };

// The standard headers (C++ up to C++23) grouped by purpose.
const Known KNOWN[] = {
    {"iostream", "I/O", "std::cout / std::cin streams"},
    {"fstream", "I/O", "file streams"},
    {"sstream", "I/O", "string streams"},
    {"iomanip", "I/O", "output manipulators"},
    {"print", "I/O", "formatted printing (C++23)"},
    {"format", "I/O", "type-safe formatting (C++20)"},
    {"ios", "I/O", "I/O base classes"},
    {"streambuf", "I/O", "stream buffers"},
    {"istream", "I/O", "input stream class"},
    {"ostream", "I/O", "output stream class"},

    {"vector", "Containers", "dynamic array"},
    {"string", "Containers", "text strings"},
    {"array", "Containers", "fixed-size array"},
    {"list", "Containers", "doubly-linked list"},
    {"forward_list", "Containers", "singly-linked list"},
    {"deque", "Containers", "double-ended queue"},
    {"map", "Containers", "ordered key-value map"},
    {"set", "Containers", "ordered unique keys"},
    {"unordered_map", "Containers", "hash map"},
    {"unordered_set", "Containers", "hash set"},
    {"queue", "Containers", "queue / priority_queue"},
    {"stack", "Containers", "stack adapter"},
    {"span", "Containers", "non-owning view (C++20)"},
    {"mdspan", "Containers", "multi-dimensional view (C++23)"},
    {"flat_map", "Containers", "flat ordered map (C++23)"},
    {"flat_set", "Containers", "flat ordered set (C++23)"},

    {"algorithm", "Algorithms", "sort, find, ..."},
    {"numeric", "Algorithms", "accumulate, iota, ..."},
    {"ranges", "Algorithms", "ranges library (C++20)"},
    {"iterator", "Algorithms", "iterator support"},

    {"memory", "Utilities", "smart pointers, allocators"},
    {"utility", "Utilities", "pair, move"},
    {"tuple", "Utilities", "heterogeneous tuple"},
    {"optional", "Utilities", "optional value"},
    {"variant", "Utilities", "type-safe union"},
    {"any", "Utilities", "any-type container"},
    {"chrono", "Utilities", "clocks and durations"},
    {"functional", "Utilities", "function objects"},
    {"bit", "Utilities", "bit manipulation (C++20)"},

    {"cmath", "Math", "sqrt, pow, sin, ..."},
    {"complex", "Math", "complex numbers"},
    {"random", "Math", "random number generators"},
    {"valarray", "Math", "numeric arrays"},
    {"ratio", "Math", "compile-time fractions"},
    {"numbers", "Math", "pi, e constants (C++20)"},

    {"thread", "Concurrency", "threads"},
    {"mutex", "Concurrency", "mutual exclusion"},
    {"shared_mutex", "Concurrency", "read/write locks"},
    {"future", "Concurrency", "promises and futures"},
    {"condition_variable", "Concurrency", "wait/notify"},
    {"atomic", "Concurrency", "atomic operations"},
    {"barrier", "Concurrency", "barrier (C++20)"},
    {"latch", "Concurrency", "latch (C++20)"},
    {"semaphore", "Concurrency", "semaphore (C++20)"},
    {"stop_token", "Concurrency", "thread cancellation (C++20)"},
    {"execution", "Concurrency", "parallel algorithm policies"},

    {"exception", "Diagnostics", "exception base class"},
    {"stdexcept", "Diagnostics", "standard exceptions"},
    {"cassert", "Diagnostics", "assert macro"},
    {"filesystem", "Diagnostics", "paths and directories"},
    {"locale", "Diagnostics", "localization"},
    {"codecvt", "Diagnostics", "character conversion"},
    {"system_error", "Diagnostics", "OS error codes"},

    {"type_traits", "Type support", "type inspection"},
    {"concepts", "Type support", "concepts (C++20)"},
    {"limits", "Type support", "numeric limits"},
    {"typeinfo", "Type support", "RTTI"},
    {"source_location", "Type support", "file/line info (C++20)"},
    {"version", "Type support", "library version macros"},

    {"cstring", "C library", "strlen, strcpy, ..."},
    {"cstdlib", "C library", "rand, malloc, exit, ..."},
    {"ctime", "C library", "C date and time"},
    {"cctype", "C library", "character classification"},
    {"cwctype", "C library", "wide character classification"},
    {"cstdint", "C library", "fixed-width integers"},
    {"cfloat", "C library", "float limits"},
    {"climits", "C library", "integer limits"},
    {"cstddef", "C library", "size_t, nullptr_t"},
    {"cstdio", "C library", "printf, scanf"},

    // classic C headers and the common all-in-one GCC header
    {"stdio.h", "C library", "printf, scanf"},
    {"stdlib.h", "C library", "rand, malloc, exit"},
    {"string.h", "C library", "strlen, strcpy"},
    {"math.h", "C library", "sqrt, pow, sin"},
    {"bits/stdc++.h", "All-in-one", "whole standard library (GCC)"},
};

struct Included { std::string name; int line; const Known* info; };
std::vector<Included> included;

const Known* findKnown(const std::string& n) {
    for (size_t i = 0; i < sizeof(KNOWN) / sizeof(KNOWN[0]); i++)
        if (n == KNOWN[i].name) return &KNOWN[i];
    return NULL;
}

}  // namespace

void includeHeader(const std::string& name, bool angle, int line) {
    if (!angle) {      // "file.h": a user header; this compiler does not load files
        reportError(PH_LEXICAL, line, "cannot open include file \"" + name +
                    "\" (only standard headers in <...> are supported)");
        return;
    }
    const Known* k = findKnown(name);
    if (!k) {
        reportError(PH_LEXICAL, line, "unknown standard header <" + name + ">");
        return;
    }
    for (size_t i = 0; i < included.size(); i++)
        if (included[i].name == name) return;     // duplicate include is ignored
    Included inc; inc.name = name; inc.line = line; inc.info = k;
    included.push_back(inc);
}

bool hasHeader(const std::string& name) {
    for (size_t i = 0; i < included.size(); i++)
        if (included[i].name == name || included[i].name == "bits/stdc++.h") return true;
    return false;
}

int headerCount() { return (int)included.size(); }

void printHeaders(std::ostream& os) {
    if (included.empty()) { os << "(no headers included)\n"; return; }
    os << std::left << std::setw(20) << "Header" << std::setw(6) << "Line"
       << std::setw(14) << "Category" << "Role in this compiler\n";
    os << "------------------------------------------------------------------------\n";
    for (size_t i = 0; i < included.size(); i++) {
        const Included& h = included[i];
        std::string role = (h.name == "iostream" || h.name == "bits/stdc++.h")
                               ? "enables cout / endl" : "accepted (no effect)";
        os << std::left << std::setw(20) << ("<" + h.name + ">") << std::setw(6) << h.line
           << std::setw(14) << h.info->category << role << "  - " << h.info->purpose << "\n";
    }
}
