#include "semantic/semantic.h"
#include "symbol_table/symbol_table.h"

#include <iostream>
#include <algorithm>
#include <sstream>
#include <string>

static int errors = 0;
static int breakableDepth = 0;
static SymbolTable symbols;
static std::ostringstream symbolReport;
struct SymbolRecord { std::string name; Type type; int depth; bool initialized; };
static std::vector<SymbolRecord> symbolRecords;

static void refreshSymbolRecord(const std::string& name) {
    int depth=symbols.scopeDepth(name);
    for(auto it=symbolRecords.rbegin();it!=symbolRecords.rend();++it)
        if(it->name==name && it->depth==depth){it->initialized=symbols.isInitializedAtDepth(name,depth);return;}
}

static void writeSymbolReport() {
    symbolReport.str(""); symbolReport.clear();
    for(const auto& r:symbolRecords)
        symbolReport<<r.name<<" : "<<typeName(r.type)<<" (scope "<<r.depth<<", "<<(r.initialized?"initialized":"uninitialized")<<")\n";
}

static bool isNumeric(Type t) {
    return t == Type::Int || t == Type::Float;
}

static bool canAssign(Type target, Type source) {
    if (target == source) return true;
    // Widening conversion supported by this compiler's language subset.
    return target == Type::Float && source == Type::Int;
}

static Type literalType(const std::string& text) {
    if (text == "true" || text == "false") return Type::Bool;

    if (text.find('.') != std::string::npos ||
        text.find('e') != std::string::npos ||
        text.find('E') != std::string::npos ||
        text.find('f') != std::string::npos ||
        text.find('F') != std::string::npos ||
        text.find('d') != std::string::npos ||
        text.find('D') != std::string::npos) {
        return Type::Float;
    }
    return Type::Int;
}

static Type expr(Node* n) {
    if (!n) return Type::Error;

    if (n->kind == "literal") return literalType(n->text);

    if (n->kind == "id") {
        Type t = symbols.lookup(n->text);
        if (t == Type::Error) {
            std::cerr << "line " << n->line << ": undeclared variable '"
                      << n->text << "'\n";
            ++errors;
            return Type::Error;
        }
        if (!symbols.isInitialized(n->text)) {
            std::cerr << "line " << n->line << ": variable '" << n->text
                      << "' might not have been initialized\n";
            ++errors;
            return Type::Error;
        }
        return t;
    }

    if (n->kind == "unary") {
        Type t = expr(n->children.at(0));
        if (t == Type::Error) return Type::Error;

        if (n->text == "!") {
            if (t != Type::Bool) {
                std::cerr << "line " << n->line
                          << ": ! requires boolean operand\n";
                ++errors;
                return Type::Error;
            }
            return Type::Bool;
        }
        if (n->text == "-") {
            if (!isNumeric(t)) {
                std::cerr << "line " << n->line
                          << ": unary minus requires numeric operand\n";
                ++errors;
                return Type::Error;
            }
            return t;
        }
    }

    if (n->kind == "binary") {
        Type a = expr(n->children.at(0));
        Type b = expr(n->children.at(1));
        if (a == Type::Error || b == Type::Error) return Type::Error;

        const std::string& op = n->text;
        if (op == "&&" || op == "||") {
            if (a != Type::Bool || b != Type::Bool) {
                std::cerr << "line " << n->line
                          << ": logical operators require boolean operands\n";
                ++errors;
                return Type::Error;
            }
            return Type::Bool;
        }

        if (op == "==" || op == "!=") {
            if (a == b || (isNumeric(a) && isNumeric(b))) return Type::Bool;
            std::cerr << "line " << n->line << ": incompatible operands for '"
                      << op << "'\n";
            ++errors;
            return Type::Error;
        }

        if (op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (isNumeric(a) && isNumeric(b)) return Type::Bool;
            std::cerr << "line " << n->line
                      << ": comparison requires numeric operands\n";
            ++errors;
            return Type::Error;
        }

        if (op == "+" || op == "-" || op == "*" ||
            op == "/" || op == "%") {
            if (isNumeric(a) && isNumeric(b))
                return (a == Type::Float || b == Type::Float)
                           ? Type::Float : Type::Int;
        }

        std::cerr << "line " << n->line << ": incompatible operands for '"
                  << op << "'\n";
        ++errors;
        return Type::Error;
    }

    return Type::Error;
}

static Type declarationType(const std::string& name) {
    if (name == "int") return Type::Int;
    if (name == "float") return Type::Float;
    if (name == "boolean") return Type::Bool;
    return Type::Error;
}

static void stmt(Node* n) {
    if (!n) return;

    if (n->kind == "block") {
        symbols.enter();
        for (Node* child : n->children) stmt(child);
        symbols.leave();
        return;
    }

    if (n->kind == "decl") {
        const Type t = declarationType(n->text);
        const std::string name = n->children.at(0)->text;
        const bool hasInitializer = n->children.size() > 1;

        // Declare first so `int x = x;` cannot accidentally resolve to an
        // outer variable with the same name.
        if (!symbols.declare(name, t, false)) {
            std::cerr << "line " << n->line << ": redeclaration of '"
                      << name << "'\n";
            ++errors;
            return;
        }
        symbolRecords.push_back(SymbolRecord{name,t,symbols.scopeDepth(name),false});

        if (hasInitializer) {
            Type initType = expr(n->children[1]);
            if (initType != Type::Error && !canAssign(t, initType)) {
                std::cerr << "line " << n->line
                          << ": initializer type mismatch for '" << name << "'\n";
                ++errors;
            } else if (initType != Type::Error) {
                symbols.setInitialized(name);
                refreshSymbolRecord(name);
            }
        }
        return;
    }

    if (n->kind == "assign") {
        Type variableType = symbols.lookup(n->text);
        if (variableType == Type::Error) {
            std::cerr << "line " << n->line << ": undeclared variable '"
                      << n->text << "'\n";
            ++errors;
            // Still inspect RHS to report its independent errors.
            expr(n->children.at(0));
            return;
        }

        Type expressionType = expr(n->children.at(0));
        if (expressionType != Type::Error &&
            !canAssign(variableType, expressionType)) {
            std::cerr << "line " << n->line
                      << ": assignment type mismatch for '" << n->text << "'\n";
            ++errors;
        } else if (expressionType != Type::Error) {
            symbols.setInitialized(n->text);
            refreshSymbolRecord(n->text);
        }
        return;
    }

    if (n->kind == "inc" || n->kind == "dec") {
        Type t=symbols.lookup(n->text);
        if(t==Type::Error){std::cerr<<"line "<<n->line<<": undeclared variable '"<<n->text<<"'\n";++errors;}
        else if(!isNumeric(t)){std::cerr<<"line "<<n->line<<": increment/decrement requires numeric variable\n";++errors;}
        else if(!symbols.isInitialized(n->text)){std::cerr<<"line "<<n->line<<": variable '"<<n->text<<"' might not have been initialized\n";++errors;}
        return;
    }

    if (n->kind == "if") {
        Type condition = expr(n->children.at(0));
        if (condition != Type::Error && condition != Type::Bool) {
            std::cerr << "line " << n->line
                      << ": condition must be boolean\n";
            ++errors;
        }

        // An if-body may execute or may be skipped. With an else branch,
        // a variable is definitely initialized only if BOTH paths initialize it.
        SymbolTable before = symbols;
        stmt(n->children.at(1));
        SymbolTable thenState = symbols;

        if (n->children.size() > 2) {
            symbols = before;
            stmt(n->children.at(2));
            SymbolTable elseState = symbols;
            symbols = before;
            symbols.mergeInitialization(thenState, elseState);
        } else {
            symbols = before;
            symbols.mergeInitialization(before, thenState);
        }
        for (const auto& record : symbolRecords) if(record.depth>=0) refreshSymbolRecord(record.name);
        return;
    }

    if (n->kind == "while") {
        Type condition = expr(n->children.at(0));
        if (condition != Type::Error && condition != Type::Bool) {
            std::cerr << "line " << n->line
                      << ": condition must be boolean\n";
            ++errors;
        }

        // A while body can execute zero times, so its assignments cannot
        // establish definite initialization after the loop.
        SymbolTable before = symbols;
        ++breakableDepth;
        stmt(n->children.at(1));
        --breakableDepth;
        symbols = before;
        return;
    }

    if (n->kind == "for") {
        symbols.enter();
        if (n->children[0]) stmt(n->children[0]);
        if (n->children[1]) {
            Type condition=expr(n->children[1]);
            if(condition!=Type::Error && condition!=Type::Bool){std::cerr<<"line "<<n->line<<": for condition must be boolean\n";++errors;}
        }
        SymbolTable before=symbols;
        ++breakableDepth;
        stmt(n->children[3]);
        --breakableDepth;
        symbols=before;
        if(n->children[2]) stmt(n->children[2]);
        symbols.leave();
        return;
    }

    if (n->kind == "do_while") {
        ++breakableDepth;
        stmt(n->children[0]);
        --breakableDepth;
        Type condition=expr(n->children[1]);
        if(condition!=Type::Error && condition!=Type::Bool){std::cerr<<"line "<<n->line<<": do-while condition must be boolean\n";++errors;}
        return;
    }

    if (n->kind == "switch") {
        Type selector=expr(n->children[0]);
        if(selector!=Type::Error && !isNumeric(selector) && selector!=Type::Bool){
            std::cerr<<"line "<<n->line<<": switch selector must be numeric or boolean\n";++errors;
        }
        std::vector<std::string> seen;
        bool hasDefault=false;
        Node* cases=n->children[1];
        ++breakableDepth;
        for(Node* item:cases->children){
            if(item->kind=="case"){
                Type ct=expr(item->children[0]);
                const std::string key=item->children[0]->text;
                if(ct!=selector && !(isNumeric(ct)&&isNumeric(selector))){std::cerr<<"line "<<item->line<<": switch case type does not match selector\n";++errors;}
                if(std::find(seen.begin(),seen.end(),key)!=seen.end()){std::cerr<<"line "<<item->line<<": duplicate switch case '"<<key<<"'\n";++errors;}
                seen.push_back(key);
            } else if(item->kind=="default") {
                if(hasDefault){std::cerr<<"line "<<item->line<<": duplicate default label in switch\n";++errors;}
                hasDefault=true;
            }
            if(item->children.size()) stmt(item->children.back());
        }
        --breakableDepth;
        return;
    }

    if (n->kind == "break") {
        if (breakableDepth==0) { std::cerr<<"line "<<n->line<<": break is only valid inside a loop or switch\n"; ++errors; }
        return;
    }

    if (n->kind == "print") {
        expr(n->children.at(0));
        return;
    }

    for (Node* child : n->children) stmt(child);
}

bool analyze(Node* root) {
    errors = 0;
    breakableDepth = 0;
    symbols = SymbolTable{};
    symbolReport.str("");
    symbolReport.clear();
    symbolRecords.clear();

    if (root) {
        for (Node* child : root->children) stmt(child);
    }
    writeSymbolReport();

    if (errors > 0) {
        std::cerr << "\nSemantic analysis failed with " << errors
                  << " error(s).\n";
        return false;
    }
    return true;
}

std::string symbolTableReport() { return symbolReport.str(); }
