#pragma once
#include "common/types.h"
#include <unordered_map>
#include <vector>
class SymbolTable { std::vector<std::unordered_map<std::string,Type>> scopes{{}};
public: void enter(){scopes.emplace_back();} void leave(){if(scopes.size()>1)scopes.pop_back();}
bool declare(const std::string& n,Type t){return scopes.back().emplace(n,t).second;}
Type lookup(const std::string& n)const{for(auto i=scopes.rbegin();i!=scopes.rend();++i){auto f=i->find(n);if(f!=i->end())return f->second;}return Type::Error;}
};
