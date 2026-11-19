/*
 * symbol_table.cpp - scope stack of hash maps.
 */
#include "symbol_table/symbol_table.h"

SymbolTable::SymbolTable() {
    scopes.push_back(Scope{0, {}});    // global scope
}

void SymbolTable::enter(int scopeId) {
    scopes.push_back(Scope{scopeId, {}});
}

void SymbolTable::leave() {
    if (scopes.size() > 1) scopes.pop_back();
}

int SymbolTable::currentScopeId() const {
    return scopes.back().id;
}

int SymbolTable::depth() const {
    return static_cast<int>(scopes.size()) - 1;
}

bool SymbolTable::declare(const std::string& name, const SymbolInfo& info) {
    return scopes.back().symbols.emplace(name, info).second;
}

const SymbolInfo* SymbolTable::lookup(const std::string& name) const {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        auto found = scope->symbols.find(name);
        if (found != scope->symbols.end()) return &found->second;
    }
    return nullptr;
}

const SymbolInfo* SymbolTable::lookupCurrent(const std::string& name) const {
    auto found = scopes.back().symbols.find(name);
    return found == scopes.back().symbols.end() ? nullptr : &found->second;
}

void SymbolTable::setInitialized(const std::string& name) {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        auto found = scope->symbols.find(name);
        if (found != scope->symbols.end()) {
            found->second.initialized = true;
            return;
        }
    }
}

void SymbolTable::mergeInitialization(const SymbolTable& left, const SymbolTable& right) {
    for (std::size_t i = 0; i < scopes.size(); ++i) {
        for (auto& entry : scopes[i].symbols) {
            auto l = left.scopes[i].symbols.find(entry.first);
            auto r = right.scopes[i].symbols.find(entry.first);
            entry.second.initialized =
                l != left.scopes[i].symbols.end() && l->second.initialized &&
                r != right.scopes[i].symbols.end() && r->second.initialized;
        }
    }
}
