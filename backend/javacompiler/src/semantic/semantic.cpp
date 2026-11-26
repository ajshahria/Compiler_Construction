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
 *   - break outside a loop or switch, continue outside a loop, bad switch
 *     selector / case labels
 *
 * Bonus (manual section 14):
 *   - arrays: the size in new int[n] is a positive integer literal, the
 *     values of {1, 2} fit the element type, an index is an int, an array is
 *     only used with an index (or as a.length), a variable cannot be indexed
 *   - methods: static methods of the class (or of the bare program); a call
 *     names a method, with one argument of a fitting type per parameter; a
 *     void method cannot be used as a value; return matches the method's
 *     type, and a non-void method cannot end without a return (Java's
 *     "missing return statement"); main returns nothing. A method sees only
 *     its parameters, its own variables and the other methods (Java has no
 *     global variables). Array parameters are not supported.
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
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

SymbolTable symbols;
SymbolTable methodTable;  // only the methods: the table a method body starts from
int errorCount = 0;
int breakableDepth = 0;   // > 0 inside a loop or switch
int loopDepth = 0;        // > 0 inside a loop (for continue)
int nextScopeId = 1;
Node* currentMethod = nullptr;   // the method being checked (nullptr in main)

/* Every declaration ever made, in order (for --symtab and scope violations). */
struct SymbolRecord {
    std::string name;
    Type type;
    int scopeId;
    int depth;
    int line;
    std::string tacName;
    int length;                  // arrays: number of elements
    bool isFunction;             // methods
    std::vector<Type> params;    // methods: parameter types
    std::string function;        // the method whose local this is ("" in main)
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
    if (name == "void") return Type::Void;
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
        if (r.name == name && !r.isFunction) {
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
    if (info->isFunction) {
        semanticError(node->line, "invalid expression: method '" + node->text + "' used without a call (write " +
                                  node->text + "(...))");
        return nullptr;
    }
    if (info->length > 0) {
        semanticError(node->line, "invalid expression: array '" + node->text + "' used without an index (write " +
                                  node->text + "[i])");
        return nullptr;
    }
    node->tacName = info->tacName;
    if (!info->initialized) {
        semanticError(node->line, "variable '" + node->text + "' might not have been initialized");
        return nullptr;
    }
    return info;
}

/* Looks up the array of a[i], a[i] = v or a.length. */
const SymbolInfo* useArray(Node* node) {
    const SymbolInfo* info = symbols.lookup(node->text);
    if (!info) {
        reportUnknownName(node->text, node->line);
        return nullptr;
    }
    if (info->isFunction || info->length == 0) {
        semanticError(node->line, "invalid expression: '" + node->text + "' is not an array");
        return nullptr;
    }
    node->tacName = info->tacName;
    return info;
}

Type checkExpr(Node* n);

/* The index of a[i] must be an int. */
void checkIndex(Node* index, const std::string& array) {
    Type t = checkExpr(index);
    if (t != Type::Error && t != Type::Int)
        semanticError(index->line, "type mismatch: index of array '" + array + "' must be an int, got " + typeName(t));
}

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

/* f(a, b): one argument of a fitting type per parameter. Returns the
 * method's return type (Type::Void for a void method). */
Type checkCall(Node* n) {
    std::vector<Type> args;
    for (Node* arg : n->children) args.push_back(checkExpr(arg));
    const SymbolInfo* info = symbols.lookup(n->text);
    if (!info) {
        semanticError(n->line, "undeclared method '" + n->text + "'");
        return Type::Error;
    }
    if (!info->isFunction) {
        semanticError(n->line, "invalid expression: '" + n->text + "' is not a method and cannot be called");
        return Type::Error;
    }
    n->tacName = info->tacName;
    if (n->children.size() != info->params.size()) {
        semanticError(n->line, "method '" + n->text + "' takes " + std::to_string(info->params.size()) +
                               " argument(s) but " + std::to_string(n->children.size()) + " were given");
        return info->type;
    }
    for (std::size_t i = 0; i < n->children.size(); ++i) {
        Type t = args[i];
        if (t != Type::Error && !isAssignable(info->params[i], t))
            semanticError(n->line, "type mismatch: argument " + std::to_string(i + 1) + " of '" + n->text +
                                   "' must be " + withArticle(info->params[i]) + ", got " + withArticle(t) + " value");
    }
    return info->type;
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
    if (n->kind == "index") {
        const SymbolInfo* info = useArray(n);
        checkIndex(n->children.at(0), n->text);
        return info ? info->type : Type::Error;
    }
    if (n->kind == "length") {
        const SymbolInfo* info = useArray(n);
        if (!info) return Type::Error;
        n->tacName = std::to_string(info->length);   // a.length is a constant
        return Type::Int;
    }
    if (n->kind == "call") {
        Type t = checkCall(n);
        if (t == Type::Void) {
            semanticError(n->line, "invalid expression: void method '" + n->text + "' does not return a value");
            return Type::Error;
        }
        return t;
    }
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

/* Declares a variable (or array) in the current scope and records it.
 * Returns its TAC name, or "" for a redeclaration (reported). */
std::string declareVariable(const std::string& name, Type type, int length, int line, bool initialized) {
    if (const SymbolInfo* previous = symbols.lookupCurrent(name)) {
        semanticError(line, "redeclaration of '" + name + "' (already declared at line " +
                            std::to_string(previous->line) + " in this scope)");
        return "";
    }
    std::string tacName = chooseTacName(name, symbols.currentScopeId());
    SymbolInfo info{type, line, symbols.currentScopeId(), tacName, initialized, 0, false, {}};
    info.length = length;
    symbols.declare(name, info);
    history.push_back(SymbolRecord{name, type, symbols.currentScopeId(), symbols.depth(), line, tacName,
                                   length, false, {}, currentMethod ? currentMethod->text : ""});
    return tacName;
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

    if (symbols.lookupCurrent(name)) {
        declareVariable(name, type, 0, n->line, false);          // reports the redeclaration
        if (n->children.size() > 1 && n->children[1]->kind != "read") checkExpr(n->children[1]);
        return;
    }

    // Declared before the initializer is checked, as in Java: in `int x = x;`
    // the right-hand x is this new, still uninitialized variable.
    n->tacName = declareVariable(name, type, 0, n->line, false);

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

/* int[] a = new int[5];   int[] b = {1, 2, 3}; */
void checkArrayDeclaration(Node* n) {
    Type type = declaredType(n->text);
    const std::string& name = n->children.at(0)->text;
    Node* init = n->children.at(1);
    int length = 0;

    if (init->kind == "new_array") {
        if (init->text != n->text)
            semanticError(n->line, "type mismatch: cannot initialize " + n->text + "[] array '" + name +
                                   "' with a new " + init->text + "[] array");
        Node* size = init->children.at(0);
        Type sizeType = checkExpr(size);
        if (size->kind != "literal" || sizeType != Type::Int) {
            if (sizeType != Type::Error)
                semanticError(n->line, "array size of '" + name + "' must be an integer literal (for example new " +
                                       n->text + "[10])");
        } else {
            length = std::atoi(size->text.c_str());
            if (length <= 0) semanticError(n->line, "array '" + name + "' must have a positive size");
        }
    } else {                                     // array_init: {1, 2, 3}
        length = static_cast<int>(init->children.size());
        for (std::size_t i = 0; i < init->children.size(); ++i) {
            Type t = checkExpr(init->children[i]);
            if (t != Type::Error && !isAssignable(type, t))
                semanticError(n->line, "type mismatch: value " + std::to_string(i + 1) + " in the initializer of " +
                                       n->text + "[] array '" + name + "' is " + withArticle(t) + " value");
        }
    }
    n->tacName = declareVariable(name, type, length > 0 ? length : 1, n->line, true);
}

void checkAssignment(Node* n) {
    const SymbolInfo* target = symbols.lookup(n->text);
    Node* source = n->children.at(0);
    Type value = source->kind == "read" ? Type::Error : checkExpr(source);
    if (!target) {
        reportUnknownName(n->text, n->line);
        return;
    }
    if (target->isFunction || target->length > 0) {
        semanticError(n->line, "invalid assignment: cannot assign to " +
                               std::string(target->isFunction ? "the method '" : "the whole array '") + n->text + "'" +
                               (target->isFunction ? "" : " (assign " + n->text + "[i])"));
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

/* a[i] = v */
void checkElementAssignment(Node* n) {
    const SymbolInfo* target = useArray(n);
    checkIndex(n->children.at(0), n->text);
    Node* source = n->children.at(1);
    if (source->kind == "read") {
        if (target) checkRead(source, target->type, n->text + "[...]");
        return;
    }
    Type value = checkExpr(source);
    if (target && value != Type::Error && !isAssignable(target->type, value))
        semanticError(n->line, "invalid assignment: cannot assign " + withArticle(value) + " value to an element of " +
                               typeName(target->type) + "[] array '" + n->text + "'");
}

void checkIncrement(Node* n) {
    std::string op = n->kind == "inc" ? "++" : "--";
    if (!n->children.empty()) {                       // a[i]++
        const SymbolInfo* info = useArray(n);
        checkIndex(n->children.at(0), n->text);
        if (info && !isNumeric(info->type))
            semanticError(n->line, "invalid expression: '" + op + "' needs a numeric variable, got " + typeName(info->type));
        return;
    }
    const SymbolInfo* info = useVariable(n);
    if (info && !isNumeric(info->type))
        semanticError(n->line, "invalid expression: '" + op + "' needs a numeric variable, got " + typeName(info->type));
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
    ++loopDepth;
    checkStmt(body);
    --loopDepth;
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

void checkReturn(Node* n) {
    Type value = n->children.empty() ? Type::Void : checkExpr(n->children[0]);
    if (!currentMethod) {                            // main is void
        if (value != Type::Void && value != Type::Error)
            semanticError(n->line, "main is void: 'return' cannot return a value");
        return;
    }
    Type expected = declaredType(currentMethod->type);
    const std::string& name = currentMethod->text;
    if (expected == Type::Void && value != Type::Void)
        semanticError(n->line, "void method '" + name + "' cannot return a value");
    else if (expected != Type::Void && value == Type::Void)
        semanticError(n->line, "method '" + name + "' must return " + withArticle(expected) + " value");
    else if (value != Type::Error && value != Type::Void && !isAssignable(expected, value))
        semanticError(n->line, "type mismatch: method '" + name + "' returns " + typeName(expected) +
                               ", got " + withArticle(value) + " value");
}

/* Can execution run past the end of this statement? (Java's rule, simplified:
 * a return, a block ending in one, or an if-else whose branches both end in
 * one cannot.) */
bool canCompleteNormally(Node* n) {
    if (!n) return true;
    if (n->kind == "return") return false;
    if (n->kind == "block") return n->children.empty() || canCompleteNormally(n->children.back());
    if (n->kind == "if" && n->children.size() > 2)
        return canCompleteNormally(n->children[1]) || canCompleteNormally(n->children[2]);
    if (n->kind == "while" && n->children[0]->kind == "literal" && n->children[0]->text == "true")
        return false;                                // while (true) { ... }: leaves only by return
    return true;
}

/* A method body: its own scope with the parameters, seeing only the methods. */
void checkMethod(Node* n) {
    if (currentMethod || symbols.depth() > 0) {
        semanticError(n->line, "method '" + n->text + "' must be defined at the top level of the class");
        return;
    }
    SymbolTable outer = symbols;
    int outerBreakable = breakableDepth, outerLoops = loopDepth;
    symbols = methodTable;
    breakableDepth = loopDepth = 0;
    currentMethod = n;
    symbols.enter(nextScopeId++);
    for (Node* param : n->children.at(0)->children) {
        const std::string& type = param->text;
        if (type.size() > 2 && type.compare(type.size() - 2, 2, "[]") == 0) {
            semanticError(param->line, "array parameter '" + param->children.at(0)->text +
                                       "' is not supported (pass the elements one by one)");
            continue;
        }
        param->tacName = declareVariable(param->children.at(0)->text, declaredType(type), 0, param->line, true);
    }
    Node* body = n->children.at(1);
    for (Node* statement : body->children) checkStmt(statement);
    if (declaredType(n->type) != Type::Void && canCompleteNormally(body))
        semanticError(n->line, "missing return statement in method '" + n->text + "'");
    symbols.leave();
    currentMethod = nullptr;
    breakableDepth = outerBreakable;
    loopDepth = outerLoops;
    symbols = outer;
}

void checkStmt(Node* n) {
    if (!n) return;                                // a statement dropped by error recovery
    const std::string& k = n->kind;

    if (k == "block") {
        checkBlock(n);
    } else if (k == "decl") {
        checkDeclaration(n);
    } else if (k == "array_decl") {
        checkArrayDeclaration(n);
    } else if (k == "assign") {
        checkAssignment(n);
    } else if (k == "assign_index") {
        checkElementAssignment(n);
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
        ++loopDepth;
        checkStmt(n->children[3]);
        --loopDepth;
        --breakableDepth;
        checkStmt(n->children[2]);
        symbols = before;
        symbols.leave();
    } else if (k == "do_while") {
        // The body always runs once, so its initializations count afterwards.
        ++breakableDepth;
        ++loopDepth;
        checkStmt(n->children.at(0));
        --loopDepth;
        --breakableDepth;
        checkCondition(n->children.at(1), "do-while", n->line);
    } else if (k == "switch") {
        checkSwitch(n);
    } else if (k == "break") {
        if (breakableDepth == 0) semanticError(n->line, "break is only valid inside a loop or switch");
    } else if (k == "continue") {
        if (loopDepth == 0) semanticError(n->line, "continue is only valid inside a loop");
    } else if (k == "method") {
        checkMethod(n);
    } else if (k == "call_stmt") {
        checkCall(n->children.at(0));               // a void method may be called as a statement
    } else if (k == "return") {
        checkReturn(n);
    }
}

/* Every method of the program: in the class (after main's body) or, in the
 * bare form, among the top-level statements. */
std::vector<Node*> programMethods(Node* root) {
    std::vector<Node*> methods;
    for (std::size_t i = 1; i < root->children.size(); ++i) methods.push_back(root->children[i]);
    for (Node* statement : root->children[0]->children)
        if (statement && statement->kind == "method") methods.push_back(statement);
    return methods;
}

}  // namespace

int analyze(Node* root) {
    symbols = SymbolTable();
    methodTable = SymbolTable();
    errorCount = 0;
    breakableDepth = 0;
    loopDepth = 0;
    nextScopeId = 1;
    currentMethod = nullptr;
    history.clear();
    scanners.clear();
    scannerImportReported = false;
    if (!root || root->children.empty()) return 0;

    // Methods are declared first, so any method can call any other (and itself).
    for (Node* m : programMethods(root)) {
        if (const SymbolInfo* previous = methodTable.lookupCurrent(m->text)) {
            semanticError(m->line, "redefinition of method '" + m->text + "' (first defined at line " +
                                   std::to_string(previous->line) + ")");
            continue;
        }
        SymbolInfo info{declaredType(m->type), m->line, 0, m->text, true, 0, true, {}};
        info.isFunction = true;
        std::vector<Type> params;
        for (Node* param : m->children.at(0)->children) params.push_back(declaredType(param->text));
        info.params = params;
        methodTable.declare(m->text, info);
        symbols.declare(m->text, info);
        history.push_back(SymbolRecord{m->text, info.type, 0, 0, m->line, m->text, 0, true, params, ""});
    }

    // The program's statement list (main's body) is the global scope (scope 0).
    // Methods written above main are checked first, so the messages come out
    // in line order.
    int mainLine = root->children[0]->line;
    for (std::size_t i = 1; i < root->children.size(); ++i)
        if (root->children[i]->line < mainLine) checkMethod(root->children[i]);
    for (Node* statement : root->children[0]->children) checkStmt(statement);
    for (std::size_t i = 1; i < root->children.size(); ++i)
        if (root->children[i]->line >= mainLine) checkMethod(root->children[i]);
    return errorCount;
}

std::vector<DeclaredName> declaredVariables() {
    std::vector<DeclaredName> names;
    for (const SymbolRecord& r : history)
        names.push_back(DeclaredName{r.tacName, r.isFunction && r.type == Type::Void ? Type::Int : r.type,
                                     r.length, r.isFunction, r.function});
    return names;
}

std::string symbolTableReport() {
    // int, an array int[5], or a method int(int,float)
    std::vector<std::string> types;
    std::size_t width = 9;
    for (const SymbolRecord& r : history) {
        std::string t = typeName(r.type);
        if (r.length > 0) t += "[" + std::to_string(r.length) + "]";
        if (r.isFunction) {
            t += "(";
            for (std::size_t i = 0; i < r.params.size(); ++i) t += (i ? "," : "") + typeName(r.params[i]);
            t += ")";
        }
        width = std::max(width, t.size() + 2);
        types.push_back(t);
    }
    std::ostringstream out;
    out << std::left << std::setw(16) << "Name" << std::setw(width) << "Type" << std::setw(7) << "Scope"
        << std::setw(7) << "Depth" << std::setw(7) << "Line" << "TAC name\n";
    out << std::setw(16) << "----" << std::setw(width) << "----" << std::setw(7) << "-----"
        << std::setw(7) << "-----" << std::setw(7) << "----" << "--------\n";
    for (std::size_t i = 0; i < history.size(); ++i) {
        const SymbolRecord& r = history[i];
        out << std::setw(16) << r.name << std::setw(width) << types[i] << std::setw(7) << r.scopeId
            << std::setw(7) << r.depth << std::setw(7) << r.line << r.tacName << '\n';
    }
    return out.str();
}
