// symbol_table.h - Phase 3 helper: nested-scope symbol table.
// A stack of scopes; each scope maps name -> Symbol. Entering a block pushes
// a scope, leaving pops it, so a variable declared in a block is invisible after it.
#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <map>
#include <ostream>
#include <string>
#include <vector>
#include "types.h"

struct Symbol {
    std::string name;
    std::string uname;   // unique name used in TAC (x, x_1, ... for shadowing)
    Type type;
    int scopeId;         // unique id of the scope it belongs to (0 = global)
    int line;            // line declared
};

class SymbolTable {
public:
    SymbolTable();
    void enterScope();
    void exitScope();
    // Returns false (and the existing symbol in *existing) if already declared in the current scope.
    bool insert(const std::string& name, Type type, int line, Symbol** existing, Symbol** created);
    Symbol* lookup(const std::string& name);          // searches all open scopes, innermost first
    // A symbol that was declared in a scope that is now closed (for "scope violation" messages).
    Symbol* findClosed(const std::string& name);
    void dump(std::ostream& os) const;                // every symbol ever declared
    const std::vector<Symbol>& allSymbols() const { return history_; }

private:
    std::vector<std::map<std::string, Symbol> > scopes_;
    std::vector<int> scopeIds_;
    std::vector<Symbol> history_;
    std::map<std::string, int> nameCount_;
    int nextScopeId_;
};

#endif
