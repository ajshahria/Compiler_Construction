/*
 * semantic.cpp - checks the rules the grammar cannot express.
 *
 * Errors detected (lab manual section 4.5, plus Java rules):
 *   - undeclared variable use
 *   - redeclaration in the same scope
 *   - scope violation (using a variable after its block has closed)
 *   - type mismatch in a declaration's initializer or a condition
 *   - invalid assignment (value type does not fit the variable)
 *   - invalid expressions (operator applied to operands of the wrong type)
 *   - use of a variable that might not have been initialized (Java rule)
 *   - break outside a loop or switch, bad switch selector / case labels
 *
 * Typing rules:
 *   + - * /      numeric operands; int if both are int, otherwise float
 *   %            numeric operands; same result rule
 *   < > <= >=    numeric operands -> boolean
 *   == !=        two numbers or two booleans -> boolean
 *   && || !      boolean operands -> boolean
 *   unary -      numeric -> same type
 *   An int value may be stored in a float variable (widening); every other
 *   combination must match exactly.
 */
#include "semantic/semantic.h"
#include "imports/imports.h"
#include "symbol_table/symbol_table.h"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

SymbolTable symbols;
int errorCount = 0;
int breakableDepth = 0;   // > 0 inside a loop or switch
int nextScopeId = 1;

/* Every declaration ever made, in order (for --symtab and scope violations). */
struct SymbolRecord {
    std::string name;
    Type type;
    int scopeId;
    int depth;
    int line;
    std::string tacName;
};
std::vector<SymbolRecord> history;

/* Scanner objects (Scanner sc = new Scanner(System.in);). They only read
 * input, so they are kept apart from the variables and have no TAC name. */
struct ScannerRecord {
    std::string name;
    int line;
};
std::vector<ScannerRecord> scanners;
bool scannerImportReported = false;

void semanticError(int line, const std::string& message) {
    std::cerr << "line " << line << ": semantic error: " << message << '\n';
    ++errorCount;
}

bool isNumeric(Type t) { return t == Type::Int || t == Type::Float; }

bool isAssignable(Type target, Type value) {
    return target == value || (target == Type::Float && value == Type::Int);
}

std::string withArticle(Type t) {
    return (t == Type::Int ? "an " : "a ") + typeName(t);
}

Type declaredType(const std::string& name) {
    if (name == "int") return Type::Int;
    if (name == "float") return Type::Float;
    return Type::Bool;
}

Type literalType(const std::string& text) {
    if (text == "true" || text == "false") return Type::Bool;
    return text.find_first_of(".eE") != std::string::npos ? Type::Float : Type::Int;
}

/* A name that is not visible: either it was declared in a block that has
 * already closed (scope violation) or never declared at all. */
void reportUnknownName(const std::string& name, int line) {
    for (const SymbolRecord& r : history) {
        if (r.name == name) {
            semanticError(line, "scope violation: '" + name + "' (declared at line " +
                                std::to_string(r.line) + ") is not visible outside its block");
            return;
        }
    }
    semanticError(line, "undeclared variable '" + name + "'");
}

/* Looks up a variable that is being read. */
const SymbolInfo* useVariable(Node* node) {
    const SymbolInfo* info = symbols.lookup(node->text);
    if (!info) {
        reportUnknownName(node->text, node->line);
        return nullptr;
    }
    node->tacName = info->tacName;
    if (!info->initialized) {
        semanticError(node->line, "variable '" + node->text + "' might not have been initialized");
        return nullptr;
    }
    return info;
}

Type checkExpr(Node* n);

Type checkBinary(Node* n) {
    Type left = checkExpr(n->children.at(0));
    Type right = checkExpr(n->children.at(1));
    if (left == Type::Error || right == Type::Error) return Type::Error;

    const std::string& op = n->text;
    const std::string operands = typeName(left) + " and " + typeName(right);

    if (op == "&&" || op == "||") {
        if (left == Type::Bool && right == Type::Bool) return Type::Bool;
        semanticError(n->line, "invalid expression: logical operator '" + op +
                               "' needs boolean operands, got " + operands);
        return Type::Error;
    }
    if (op == "==" || op == "!=") {
        if (left == right || (isNumeric(left) && isNumeric(right))) return Type::Bool;
        semanticError(n->line, "invalid expression: cannot compare " + operands + " using '" + op + "'");
        return Type::Error;
    }
    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
        if (isNumeric(left) && isNumeric(right)) return Type::Bool;
        semanticError(n->line, "invalid expression: operator '" + op + "' needs numeric operands, got " + operands);
        return Type::Error;
    }
    // + - * / %
    if (isNumeric(left) && isNumeric(right))
        return (left == Type::Float || right == Type::Float) ? Type::Float : Type::Int;
    semanticError(n->line, "invalid expression: operator '" + op + "' needs numeric operands, got " + operands);
    return Type::Error;
}

Type checkUnary(Node* n) {
    Type operand = checkExpr(n->children.at(0));
    if (operand == Type::Error) return Type::Error;
    if (n->text == "!") {
        if (operand == Type::Bool) return Type::Bool;
        semanticError(n->line, "invalid expression: operator '!' needs a boolean operand, got " + typeName(operand));
        return Type::Error;
    }
    if (isNumeric(operand)) return operand;
    semanticError(n->line, "invalid expression: unary '-' needs a numeric operand, got " + typeName(operand));
    return Type::Error;
}

/* Computes the type of an expression. */
Type checkExpr(Node* n) {
    if (!n) return Type::Error;
    if (n->kind == "literal") return literalType(n->text);
    if (n->kind == "id") {
        const SymbolInfo* info = useVariable(n);
        return info ? info->type : Type::Error;
    }
    if (n->kind == "unary") return checkUnary(n);
    if (n->kind == "binary") return checkBinary(n);
    if (n->kind == "read") {   // a decl/assign with a read as its whole value never gets here
        semanticError(n->line, n->text + "() can only be used alone on the right of '=' "
                               "(for example: int n = " + n->text + "();)");
        return Type::Error;
    }
    return Type::Error;
}

/* Scanner sc = new Scanner(System.in); */
void checkScannerDeclaration(Node* n) {
    if (!isImported("java.util", "Scanner") && !scannerImportReported) {
        semanticError(n->line, "cannot find symbol 'Scanner': add 'import java.util.Scanner;' "
                               "(or 'import java.util.*;') at the top of the file");
        scannerImportReported = true;    // once, not for every Scanner
    }
    for (const ScannerRecord& s : scanners) {
        if (s.name == n->text) {
            semanticError(n->line, "redeclaration of Scanner '" + n->text + "' (already declared at line " +
                                   std::to_string(s.line) + ")");
            return;
        }
    }
    if (const SymbolInfo* previous = symbols.lookupCurrent(n->text)) {
        semanticError(n->line, "redeclaration of '" + n->text + "' (already declared at line " +
                               std::to_string(previous->line) + " in this scope)");
        return;
    }
    scanners.push_back(ScannerRecord{n->text, n->line});
}

/* The value of `x = sc.nextInt();`: sc must be a Scanner and the method must
 * read x's type exactly (the TAC 'read x' reads a value of x's type). Returns
 * the target type when it is correct, Type::Error otherwise. */
Type checkRead(Node* read, Type target, const std::string& variable) {
    const std::string& call = read->text;
    std::string::size_type dot = call.find('.');
    std::string scanner = call.substr(0, dot), method = call.substr(dot + 1);

    bool known = false;
    for (const ScannerRecord& s : scanners) known = known || s.name == scanner;
    if (!known) {
        if (symbols.lookup(scanner))
            semanticError(read->line, "'" + scanner + "' is a variable, not a Scanner: '" + method +
                                      "()' can only be called on a Scanner");
        else
            semanticError(read->line, "undeclared Scanner '" + scanner + "' (declare it first: Scanner " +
                                      scanner + " = new Scanner(System.in);)");
        return Type::Error;
    }
    Type reads = method == "nextInt" ? Type::Int : method == "nextBoolean" ? Type::Bool : Type::Float;
    if (reads != target) {
        std::string right = target == Type::Int ? "nextInt" : target == Type::Bool ? "nextBoolean" : "nextFloat";
        semanticError(read->line, "type mismatch: " + call + "() reads " + withArticle(reads) + ", but '" +
                                  variable + "' is " + withArticle(target) + " (use " + scanner + "." + right + "())");
        return Type::Error;
    }
    return target;
}

void checkCondition(Node* condition, const std::string& keyword, int line) {
    Type t = checkExpr(condition);
    if (t != Type::Error && t != Type::Bool)
        semanticError(line, "type mismatch: " + keyword + " condition must be boolean, got " + typeName(t));
}

/* Picks the name used in TAC. All variables share one flat namespace in TAC,
 * so a variable whose name was already used by an earlier declaration (for
 * example one that shadows an outer variable) becomes name@scope. '@' cannot
 * appear in a source identifier, so the new name never clashes. */
std::string chooseTacName(const std::string& name, int scopeId) {
    for (const SymbolRecord& r : history)
        if (r.name == name) return name + "@" + std::to_string(scopeId);
    return name;
}

void checkStmt(Node* n);

void checkBlock(Node* block) {
    symbols.enter(nextScopeId++);
    for (Node* child : block->children) checkStmt(child);
    symbols.leave();
}

void checkDeclaration(Node* n) {
    Type type = declaredType(n->text);
    const std::string& name = n->children.at(0)->text;

    if (const SymbolInfo* previous = symbols.lookupCurrent(name)) {
        semanticError(n->line, "redeclaration of '" + name + "' (already declared at line " +
                               std::to_string(previous->line) + " in this scope)");
        if (n->children.size() > 1 && n->children[1]->kind != "read") checkExpr(n->children[1]);
        return;
    }

    // Declared before the initializer is checked, as in Java: in `int x = x;`
    // the right-hand x is this new, still uninitialized variable.
    std::string tacName = chooseTacName(name, symbols.currentScopeId());
    symbols.declare(name, SymbolInfo{type, n->line, symbols.currentScopeId(), tacName, false});
    history.push_back(SymbolRecord{name, type, symbols.currentScopeId(), symbols.depth(), n->line, tacName});
    n->tacName = tacName;

    if (n->children.size() > 1) {
        Node* init = n->children[1];
        Type value = init->kind == "read" ? checkRead(init, type, name) : checkExpr(init);
        if (value == Type::Error) return;
        if (!isAssignable(type, value))
            semanticError(n->line, "type mismatch: cannot initialize " + typeName(type) + " variable '" +
                                   name + "' with " + withArticle(value) + " value");
        else
            symbols.setInitialized(name);
    }
}

void checkAssignment(Node* n) {
    const SymbolInfo* target = symbols.lookup(n->text);
    Node* source = n->children.at(0);
    Type value = source->kind == "read" ? Type::Error : checkExpr(source);
    if (!target) {
        reportUnknownName(n->text, n->line);
        return;
    }
    if (source->kind == "read") value = checkRead(source, target->type, n->text);
    n->tacName = target->tacName;
    if (value == Type::Error) return;
    if (!isAssignable(target->type, value))
        semanticError(n->line, "invalid assignment: cannot assign " + withArticle(value) + " value to " +
                               typeName(target->type) + " variable '" + n->text + "'");
    else
        symbols.setInitialized(n->text);
}

void checkIncrement(Node* n) {
    const SymbolInfo* info = useVariable(n);
    if (info && !isNumeric(info->type))
        semanticError(n->line, "invalid expression: '" + std::string(n->kind == "inc" ? "++" : "--") +
                               "' needs a numeric variable, got " + typeName(info->type));
}

void checkIf(Node* n) {
    checkCondition(n->children.at(0), "if", n->line);

    // A variable is definitely initialized after the if only if every path
    // through it initializes the variable.
    SymbolTable before = symbols;
    checkStmt(n->children.at(1));
    SymbolTable afterThen = symbols;
    symbols = before;
    if (n->children.size() > 2) {
        checkStmt(n->children.at(2));
        SymbolTable afterElse = symbols;
        symbols = before;
        symbols.mergeInitialization(afterThen, afterElse);
    } else {
        symbols.mergeInitialization(before, afterThen);
    }
}

/* Loop bodies may run zero times, so they cannot initialize variables for
 * the code after the loop. */
void checkLoopBody(Node* body) {
    SymbolTable before = symbols;
    ++breakableDepth;
    checkStmt(body);
    --breakableDepth;
    symbols = before;
}

void checkSwitch(Node* n) {
    Type selector = checkExpr(n->children.at(0));
    if (selector != Type::Error && selector != Type::Int && selector != Type::Bool)
        semanticError(n->line, "type mismatch: switch selector must be int or boolean, got " + typeName(selector));

    std::vector<std::string> seen;
    bool hasDefault = false;
    SymbolTable before = symbols;
    ++breakableDepth;
    for (Node* clause : n->children.at(1)->children) {
        if (clause->kind == "case") {
            Node* label = clause->children.at(0);
            Type labelType = checkExpr(label);
            if (selector != Type::Error && labelType != selector)
                semanticError(clause->line, "type mismatch: case label " + label->text +
                                            " does not match the " + typeName(selector) + " selector");
            if (std::find(seen.begin(), seen.end(), label->text) != seen.end())
                semanticError(clause->line, "duplicate case label '" + label->text + "'");
            seen.push_back(label->text);
        } else if (hasDefault) {
            semanticError(clause->line, "duplicate default label in switch");
        } else {
            hasDefault = true;
        }
        checkBlock(clause->children.back());
    }
    --breakableDepth;
    symbols = before;
}

void checkStmt(Node* n) {
    if (!n) return;                                // a statement dropped by error recovery
    const std::string& k = n->kind;

    if (k == "block") {
        checkBlock(n);
    } else if (k == "decl") {
        checkDeclaration(n);
    } else if (k == "assign") {
        checkAssignment(n);
    } else if (k == "inc" || k == "dec") {
        checkIncrement(n);
    } else if (k == "print") {
        checkExpr(n->children.at(0));
    } else if (k == "scanner") {
        checkScannerDeclaration(n);
    } else if (k == "if") {
        checkIf(n);
    } else if (k == "while") {
        checkCondition(n->children.at(0), "while", n->line);
        checkLoopBody(n->children.at(1));
    } else if (k == "for") {
        // for (init; condition; update) body - the header has its own scope.
        symbols.enter(nextScopeId++);
        checkStmt(n->children[0]);
        if (n->children[1]) checkCondition(n->children[1], "for", n->line);
        SymbolTable before = symbols;
        ++breakableDepth;
        checkStmt(n->children[3]);
        --breakableDepth;
        checkStmt(n->children[2]);
        symbols = before;
        symbols.leave();
    } else if (k == "do_while") {
        // The body always runs once, so its initializations count afterwards.
        ++breakableDepth;
        checkStmt(n->children.at(0));
        --breakableDepth;
        checkCondition(n->children.at(1), "do-while", n->line);
    } else if (k == "switch") {
        checkSwitch(n);
    } else if (k == "break") {
        if (breakableDepth == 0) semanticError(n->line, "break is only valid inside a loop or switch");
    }
}

}  // namespace

int analyze(Node* root) {
    symbols = SymbolTable();
    errorCount = 0;
    breakableDepth = 0;
    nextScopeId = 1;
    history.clear();
    scanners.clear();
    scannerImportReported = false;

    // The program's statement list is the global scope (scope 0).
    if (root && !root->children.empty())
        for (Node* statement : root->children[0]->children) checkStmt(statement);
    return errorCount;
}

std::vector<std::pair<std::string, Type>> declaredVariables() {
    std::vector<std::pair<std::string, Type>> variables;
    for (const SymbolRecord& r : history) variables.emplace_back(r.tacName, r.type);
    return variables;
}

std::string symbolTableReport() {
    std::ostringstream out;
    out << std::left << std::setw(16) << "Name" << std::setw(9) << "Type" << std::setw(7) << "Scope"
        << std::setw(7) << "Depth" << std::setw(7) << "Line" << "TAC name\n";
    out << std::setw(16) << "----" << std::setw(9) << "----" << std::setw(7) << "-----"
        << std::setw(7) << "-----" << std::setw(7) << "----" << "--------\n";
    for (const SymbolRecord& r : history)
        out << std::setw(16) << r.name << std::setw(9) << typeName(r.type) << std::setw(7) << r.scopeId
            << std::setw(7) << r.depth << std::setw(7) << r.line << r.tacName << '\n';
    return out.str();
}
