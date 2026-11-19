#include "symbol_table.h"
#include <iomanip>
#include <sstream>

SymbolTable::SymbolTable() : nextScopeId_(0) {
    enterScope();   // global scope, id 0
}

void SymbolTable::enterScope() {
    scopes_.push_back(std::map<std::string, Symbol>());
    scopeIds_.push_back(nextScopeId_++);
}

void SymbolTable::exitScope() {
    scopes_.pop_back();
    scopeIds_.pop_back();
}

bool SymbolTable::insert(const std::string& name, Type type, int line,
                         Symbol** existing, Symbol** created) {
    std::map<std::string, Symbol>& cur = scopes_.back();
    std::map<std::string, Symbol>::iterator it = cur.find(name);
    if (it != cur.end()) {
        if (existing) *existing = &it->second;
        return false;
    }
    Symbol s;
    s.name = name;
    s.type = type;
    s.scopeId = scopeIds_.back();
    s.line = line;
    int n = nameCount_[name]++;          // 0 for the first 'x', 1 for the next ...
    if (n == 0) {
        s.uname = name;
    } else {
        std::ostringstream ss;
        ss << name << "_" << n;
        s.uname = ss.str();
    }
    cur[name] = s;
    history_.push_back(s);
    if (created) *created = &cur[name];
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (int i = (int)scopes_.size() - 1; i >= 0; i--) {
        std::map<std::string, Symbol>::iterator it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return &it->second;
    }
    return NULL;
}

Symbol* SymbolTable::findClosed(const std::string& name) {
    for (size_t i = 0; i < history_.size(); i++) {
        if (history_[i].name != name) continue;
        bool open = false;
        for (size_t j = 0; j < scopeIds_.size(); j++)
            if (scopeIds_[j] == history_[i].scopeId) open = true;
        if (!open) return &history_[i];
    }
    return NULL;
}

void SymbolTable::dump(std::ostream& os) const {
    os << std::left << std::setw(10) << "Name" << std::setw(10) << "Type"
       << std::setw(8) << "Scope" << "Line\n";
    os << "------------------------------------\n";
    for (size_t i = 0; i < history_.size(); i++) {
        const Symbol& s = history_[i];
        os << std::left << std::setw(10) << s.uname << std::setw(10) << typeName(s.type)
           << std::setw(8) << s.scopeId << s.line << "\n";
    }
}
