/*
 * symbol_table.h - scoped symbol table.
 *
 * Scopes form a stack: the global scope is at the bottom and every block
 * ({ ... }, a for header, ...) pushes a new one. Lookup searches from the
 * innermost scope outwards, so inner declarations shadow outer ones, and a
 * scope's names stop being visible when the block ends.
 *
 * Each entry also tracks whether the variable is definitely initialized
 * (Java's "might not have been initialized" rule). The semantic analyzer
 * copies the whole table before an if/while body and merges the copies
 * afterwards, so the table is a small value type.
 */
#pragma once

#include "common/types.h"

#include <string>
#include <unordered_map>
#include <vector>

struct SymbolInfo {
    Type type;
    int line;               // line where it was declared
    int scopeId;            // unique id of the declaring scope (0 = global)
    std::string tacName;    // name used in the TAC (see semantic.cpp)
    bool initialized;
    int length = 0;                  // arrays (bonus): number of elements
    bool isFunction = false;         // methods (bonus)
    std::vector<Type> params;        // methods: parameter types
};

class SymbolTable {
public:
    SymbolTable();

    void enter(int scopeId);           // push a new scope
    void leave();                      // pop the innermost scope (never the global one)
    int currentScopeId() const;
    int depth() const;                 // 0 = global scope

    /* Adds name to the current scope; false if it is already declared there. */
    bool declare(const std::string& name, const SymbolInfo& info);

    /* Visible declaration of name (inner to outer), or nullptr. */
    const SymbolInfo* lookup(const std::string& name) const;

    /* Declaration of name in the current scope only, or nullptr. */
    const SymbolInfo* lookupCurrent(const std::string& name) const;

    void setInitialized(const std::string& name);

    /* After an if/else: a variable is initialized only if it was initialized
     * on both paths. `left` and `right` must have the same scopes as *this. */
    void mergeInitialization(const SymbolTable& left, const SymbolTable& right);

private:
    struct Scope {
        int id;
        std::unordered_map<std::string, SymbolInfo> symbols;
    };
    std::vector<Scope> scopes;
};
