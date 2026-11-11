// headers.h - handles '#include <header>' lines found by the lexer.
// All C++ standard library headers are recognised. Only <iostream> has an effect in this
// compiler (it makes 'cout' / 'endl' legal); the others are accepted and listed.
#ifndef HEADERS_H
#define HEADERS_H

#include <ostream>
#include <string>

// Called by the lexer for every #include. Reports an error for unknown headers.
void includeHeader(const std::string& name, bool angle, int line);
bool hasHeader(const std::string& name);
int  headerCount();
// Table of every included header: name, line, category, what it does here.
void printHeaders(std::ostream& os);

#endif
