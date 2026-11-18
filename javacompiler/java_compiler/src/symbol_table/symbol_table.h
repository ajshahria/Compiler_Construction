
#pragma once

#include "common/types.h"
#include <string>
#include <unordered_map>
#include <vector>

struct SymbolInfo {
    Type type;
    bool initialized;
};

class SymbolTable {
private:
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes{{}};

public:
    void enter() {
        scopes.emplace_back();
    }

    void leave() {
        if (scopes.size() > 1)
            scopes.pop_back();
    }

    bool declare(const std::string& name, Type type,
                 bool initialized = false) {
        return scopes.back().emplace(
            name, SymbolInfo{type, initialized}
        ).second;
    }

    Type lookup(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second.type;
        }
        return Type::Error;
    }

    bool isInitialized(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second.initialized;
        }
        return false;
    }

    void setInitialized(const std::string& name, bool value = true) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second.initialized = value;
                return;
            }
        }
    }

    void mergeInitialization(
        const SymbolTable& left,
        const SymbolTable& right
    ) {
        for (std::size_t i = 0; i < scopes.size(); ++i) {
            for (auto& entry : scopes[i]) {
                auto l = left.scopes[i].find(entry.first);
                auto r = right.scopes[i].find(entry.first);

                entry.second.initialized =
                    l != left.scopes[i].end() &&
                    r != right.scopes[i].end() &&
                    l->second.initialized &&
                    r->second.initialized;
            }
        }
    }
};
