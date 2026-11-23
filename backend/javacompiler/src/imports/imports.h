// imports.h - handles the 'import ...;' lines found by the lexer (MiniJava's
// counterpart of C/C++ '#include'). Standard library imports (java.* and
// javax.*) are accepted and listed by --headers; none of them changes what
// this compiler accepts, since MiniJava only uses built-in types and
// System.out.println. Any other package is reported as a lexical error.
#ifndef IMPORTS_H
#define IMPORTS_H

#include <ostream>
#include <string>

// Called by the lexer for every import line. 'name' is the imported name
// (e.g. "java.util.Scanner" or "java.util.*"), 'isStatic' is true for
// 'import static ...'. Reports an error for a non-standard package.
void addImport(const std::string& name, bool isStatic, int line);

// True if the class java.<package>.<name> was imported, by name or with <package>.*
// (e.g. isImported("java.util", "Scanner")).
bool isImported(const std::string& package, const std::string& name);

// --headers: table of every import (name, line, package, what it is for).
void printImports(std::ostream& out);

#endif
