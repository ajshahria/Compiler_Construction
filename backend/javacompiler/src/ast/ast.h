/*
 * ast.h - Abstract Syntax Tree of MiniJava.
 *
 * Every node has a kind, an optional text (identifier name, operator, literal
 * value or type name), its source line and its children. Parentheses,
 * semicolons and the class/main wrapper are not stored.
 *
 *   kind       text            children
 *   ---------  --------------  --------------------------------------------
 *   program    class name      block
 *   block      -               statements (a new scope)
 *   decl       type name       id [, initializer]
 *   assign     variable name   value
 *   inc / dec  variable name   -
 *   print      -               value
 *   scanner    scanner name    -        (Scanner sc = new Scanner(System.in);)
 *   read       sc.nextInt ...  -        (only as the whole value of a decl/assign)
 *   if         -               condition, then [, else]
 *   while      -               condition, body
 *   for        -               init?, condition?, update?, body   (may be null)
 *   do_while   -               body, condition
 *   switch     -               selector, cases
 *   cases      -               case / default clauses
 *   case       -               literal, block
 *   default    -               block
 *   break      -               -
 *   continue   -               -                              (bonus)
 *   array_decl element type    id, new_array(size literal) or array_init(values)  (bonus)
 *   index      array name      index                 a[i]     (bonus)
 *   assign_index array name    index, value          a[i] = v (bonus)
 *   length     array name      -                     a.length (bonus)
 *   method     name            params, body    (type: return type)          (bonus)
 *   params     -               param ...
 *   param      type            id
 *   call       method name     arguments             f(a, b)  (bonus)
 *   call_stmt  -               call                  f(a);    (bonus)
 *   return     -               [value]                        (bonus)
 *   inc / dec  variable name   [index]   (an index for a[i]++)
 *   binary     operator        left, right
 *   unary      operator        operand
 *   literal    value           -
 *   id         name            -
 */
#pragma once

#include <ostream>
#include <string>
#include <vector>

struct Node {
    std::string kind;
    std::string text;
    int line = 1;
    std::vector<Node*> children;
    std::string tacName;   // for id/decl/assign/inc/dec: unique name chosen by the semantic analyzer
    std::string type;      // method: return type (int, float, boolean or void)
};

Node* makeNode(const char* kind, const char* text, int line,
               Node* a = nullptr, Node* b = nullptr, Node* c = nullptr);
void freeTree(Node* node);

/* Prints the tree with indentation (the --ast option). */
void printAst(Node* node, std::ostream& out, int depth = 0);

/* Prints the tree as a Graphviz digraph (the --dot option, bonus). */
void printDot(Node* root, std::ostream& out);
