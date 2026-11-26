/*
 * tac.cpp - Three Address Code generator.
 *
 * Every instruction has at most one operator and three addresses:
 *     x = y              copy
 *     t1 = a + b         binary operation
 *     t2 = - a           unary operation (- or !)
 *     ifFalse c goto L1  conditional jump (also: if c goto L1)
 *     goto L1            unconditional jump
 *     L1:                label
 *     print x            output
 *
 * Expressions are generated in post-order: operands first, then the operator,
 * whose result goes into a new temporary t1, t2, ... . && and || short-circuit:
 * the right operand is only evaluated when it can change the result.
 * Variables use the unique TAC name chosen by the semantic analyzer, so a
 * variable that shadows an outer one (x and x@3) never shares its storage.
 *
 * Control flow:
 *   if (c) S1 else S2       while (c) S
 *     <c>                   L1:
 *     ifFalse c goto L1       <c>
 *     <S1>                    ifFalse c goto L2
 *     goto L2                 <S>
 *   L1:                       goto L1
 *     <S2>                  L2:
 *   L2:
 *
 * for, do-while and switch follow the same pattern; break jumps to the end
 * label of the innermost loop or switch, continue to the place where the
 * innermost loop tests its condition (for: its update).
 *
 * Bonus arrays and methods:
 *     t1 = a[i]          read an element (i is an element index)
 *     a[i] = t1          write an element       read a[i]   (a[i] = sc.nextInt();)
 *     func f:            a method: its code only runs through a call
 *     formal x           the next argument goes into parameter x
 *     return t1          leave the method (return: without a value)
 *     endfunc            end of the method
 *     param t1           pass an argument, then
 *     t2 = call f 1      call f with the last 1 param (call f 1: no value used)
 * a.length is the array's size, a constant. A return in main jumps to the end
 * of main.
 */
#include "codegen/tac.h"

#include <iostream>
#include <ostream>
#include <string>
#include <vector>

class TACGenerator {

private:
    std::ostream& out;

    int tempCount = 0;
    int labelCount = 0;
    std::vector<std::string> breakLabels;
    std::vector<std::string> continueLabels;
    bool inMethod = false;
    std::string mainEndLabel;   // made by the first return in main

    // Does a loop body contain a continue for this loop (not for a nested loop)?
    static bool containsContinue(Node* node) {
        if (!node) return false;
        if (node->kind == "continue") return true;
        if (node->kind == "while" || node->kind == "for" || node->kind == "do_while" || node->kind == "method")
            return false;
        for (Node* child : node->children)
            if (containsContinue(child)) return true;
        return false;
    }

    // A call: the arguments are computed first (one may itself contain a
    // call), then passed with param. Returns the temporary with the result,
    // or "" when the value is not wanted.
    std::string generateCall(Node* node, bool wantValue) {
        std::vector<std::string> args;
        for (Node* arg : node->children) args.push_back(generateExpression(arg));
        for (const std::string& arg : args) out << "param " << arg << '\n';
        if (!wantValue) {
            out << "call " << node->text << ' ' << args.size() << '\n';
            return "";
        }
        std::string temp = newTemp();
        out << temp << " = call " << node->text << ' ' << args.size() << '\n';
        return temp;
    }

    // Generate temporary variable
    std::string newTemp() {
        return "t" + std::to_string(++tempCount);
    }

    // Generate label
    std::string newLabel() {
        return "L" + std::to_string(++labelCount);
    }

    // Generate expression code
    std::string generateExpression(Node* node) {

        if (!node) {
            return "?";
        }

        // Literal, or variable under its unique TAC name
        if (node->kind == "literal") {
            return node->text;
        }
        if (node->kind == "id") {
            return node->tacName.empty() ? node->text : node->tacName;
        }
        if (node->kind == "length") {           // a constant: the size of the array
            return node->tacName;
        }
        if (node->kind == "index") {
            std::string index = generateExpression(node->children[0]);
            std::string temp = newTemp();
            out << temp << " = " << node->tacName << '[' << index << "]\n";
            return temp;
        }
        if (node->kind == "call") {
            return generateCall(node, true);
        }

        // Unary expression
        if (node->kind == "unary") {

            std::string operand =
                generateExpression(node->children[0]);

            std::string temp = newTemp();

            out << temp << " = "
                << node->text << " " << operand << '\n';

            return temp;
        }

        // Binary expression
        if (node->kind == "binary") {

            // Short-circuit logical AND
            if (node->text == "&&") {

                std::string falseLabel = newLabel();
                std::string endLabel = newLabel();

                std::string left =
                    generateExpression(node->children[0]);

                out << "ifFalse " << left
                    << " goto " << falseLabel << '\n';

                std::string right =
                    generateExpression(node->children[1]);

                out << "ifFalse " << right
                    << " goto " << falseLabel << '\n';

                std::string result = newTemp();

                out << result << " = true\n";

                out << "goto " << endLabel << '\n';

                out << falseLabel << ":\n";
                out << result << " = false\n";

                out << endLabel << ":\n";

                return result;
            }

            // Short-circuit logical OR
            if (node->text == "||") {

                std::string trueLabel = newLabel();
                std::string endLabel = newLabel();

                std::string left =
                    generateExpression(node->children[0]);

                out << "if " << left
                    << " goto " << trueLabel << '\n';

                std::string right =
                    generateExpression(node->children[1]);

                out << "if " << right
                    << " goto " << trueLabel << '\n';

                std::string result = newTemp();

                out << result << " = false\n";

                out << "goto " << endLabel << '\n';

                out << trueLabel << ":\n";
                out << result << " = true\n";

                out << endLabel << ":\n";

                return result;
            }

            // Normal binary operation
            std::string left =
                generateExpression(node->children[0]);

            std::string right =
                generateExpression(node->children[1]);

            std::string temp = newTemp();

            out << temp << " = "
                << left << " "
                << node->text << " "
                << right << '\n';

            return temp;
        }

        std::cerr << "TAC error: unsupported expression node: "
                  << node->kind << '\n';

        return "?";
    }

    // Generate statement code
    void generateStatement(Node* node) {

        if (!node) {
            return;
        }

        // Program: main's body, then the methods of the class
        if (node->kind == "program") {
            generateStatement(node->children[0]);
            if (!mainEndLabel.empty()) out << mainEndLabel << ":\n";
            for (std::size_t i = 1; i < node->children.size(); ++i)
                generateStatement(node->children[i]);
            return;
        }
        if (node->kind == "method") {
            out << "func " << node->text << ":\n";
            for (Node* param : node->children[0]->children)
                out << "formal " << param->tacName << '\n';
            bool outer = inMethod;
            inMethod = true;
            generateStatement(node->children[1]);
            inMethod = outer;
            out << "endfunc\n";
            return;
        }
        if (node->kind == "call_stmt") {
            generateCall(node->children[0], false);
            return;
        }
        if (node->kind == "return") {
            if (inMethod) {
                if (node->children.empty()) out << "return\n";
                else out << "return " << generateExpression(node->children[0]) << '\n';
            } else {                          // main: go to its end
                if (mainEndLabel.empty()) mainEndLabel = newLabel();
                out << "goto " << mainEndLabel << '\n';
            }
            return;
        }
        if (node->kind == "continue") {
            if (!continueLabels.empty()) out << "goto " << continueLabels.back() << '\n';
            return;
        }
        if (node->kind == "array_decl") {     // int[] a = {1, 2}: one store per value
            Node* init = node->children[1];
            if (init->kind == "array_init")
                for (std::size_t i = 0; i < init->children.size(); ++i) {
                    std::string value = generateExpression(init->children[i]);
                    out << node->tacName << '[' << i << "] = " << value << '\n';
                }
            return;
        }
        if (node->kind == "assign_index") {
            std::string index = generateExpression(node->children[0]);
            if (node->children[1]->kind == "read") {
                out << "read " << node->tacName << '[' << index << "]\n";
                return;
            }
            std::string value = generateExpression(node->children[1]);
            out << node->tacName << '[' << index << "] = " << value << '\n';
            return;
        }
        // Block
        if (node->kind == "block") {

            for (Node* child : node->children) {
                generateStatement(child);
            }

            return;
        }

        // Variable declaration
        if (node->kind == "decl") {

            if (node->children.size() > 1 && node->children[1]->kind == "read") {
                out << "read " << node->tacName << '\n';   // int n = sc.nextInt();
            } else if (node->children.size() > 1) {

                std::string result =
                    generateExpression(node->children[1]);

                out << node->tacName
                    << " = " << result << '\n';
            }

            return;
        }

        // Assignment
        if (node->kind == "assign" && node->children[0]->kind == "read") {
            out << "read " << node->tacName << '\n';       // n = sc.nextInt();
            return;
        }

        if (node->kind == "assign") {

            std::string result =
                generateExpression(node->children[0]);

            out << node->tacName
                << " = " << result << '\n';

            return;
        }

        if ((node->kind == "inc" || node->kind == "dec") && !node->children.empty()) {   // a[i]++
            std::string index = generateExpression(node->children[0]);
            std::string old = newTemp(), result = newTemp();
            out << old << " = " << node->tacName << '[' << index << "]\n";
            out << result << " = " << old << (node->kind == "inc" ? " + 1\n" : " - 1\n");
            out << node->tacName << '[' << index << "] = " << result << '\n';
            return;
        }
        if (node->kind == "inc" || node->kind == "dec") {
            out << node->tacName << " = " << node->tacName << (node->kind == "inc" ? " + 1\n" : " - 1\n");
            return;
        }

        // Print statement
        if (node->kind == "print") {

            std::string result =
                generateExpression(node->children[0]);

            out << "print " << result << '\n';

            return;
        }

        // If / Else
        if (node->kind == "if") {

            std::string falseLabel = newLabel();

            std::string condition =
                generateExpression(node->children[0]);

            out << "ifFalse " << condition
                << " goto " << falseLabel << '\n';

            generateStatement(node->children[1]);

            // If an else branch exists
            if (node->children.size() > 2) {

                std::string endLabel = newLabel();

                out << "goto " << endLabel << '\n';

                out << falseLabel << ":\n";

                generateStatement(node->children[2]);

                out << endLabel << ":\n";
            }
            else {

                out << falseLabel << ":\n";
            }

            return;
        }

        // While loop
        if (node->kind == "while") {

            std::string startLabel = newLabel();
            std::string endLabel = newLabel();

            out << startLabel << ":\n";

            std::string condition =
                generateExpression(node->children[0]);

            out << "ifFalse " << condition
                << " goto " << endLabel << '\n';

            breakLabels.push_back(endLabel);
            continueLabels.push_back(startLabel);
            generateStatement(node->children[1]);
            continueLabels.pop_back();
            breakLabels.pop_back();

            out << "goto " << startLabel << '\n';

            out << endLabel << ":\n";

            return;
        }

        if (node->kind == "for") {
            if(node->children[0]) generateStatement(node->children[0]);
            std::string start=newLabel(), end=newLabel();
            out<<start<<":\n";
            if(node->children[1]){std::string c=generateExpression(node->children[1]);out<<"ifFalse "<<c<<" goto "<<end<<'\n';}
            std::string next = containsContinue(node->children[3]) ? newLabel() : "";
            breakLabels.push_back(end);
            continueLabels.push_back(next);
            generateStatement(node->children[3]);
            continueLabels.pop_back();
            breakLabels.pop_back();
            if(!next.empty()) out<<next<<":\n";
            if(node->children[2]) generateStatement(node->children[2]);
            out<<"goto "<<start<<'\n'<<end<<":\n";
            return;
        }

        if (node->kind == "do_while") {
            std::string start=newLabel(), end=newLabel();
            out<<start<<":\n";
            std::string next = containsContinue(node->children[0]) ? newLabel() : "";
            breakLabels.push_back(end);
            continueLabels.push_back(next);
            generateStatement(node->children[0]);
            continueLabels.pop_back();
            breakLabels.pop_back();
            if(!next.empty()) out<<next<<":\n";
            std::string c=generateExpression(node->children[1]);
            out<<"if "<<c<<" goto "<<start<<'\n'<<end<<":\n";
            return;
        }

        if (node->kind == "switch") {
            std::string selector=generateExpression(node->children[0]);
            Node* cases=node->children[1];
            std::string end=newLabel(), defaultLabel=end;
            std::vector<std::pair<Node*,std::string>> labels;
            for(Node* item:cases->children){
                if(item->kind=="default") defaultLabel=newLabel();
                else labels.push_back({item,newLabel()});
            }
            for(const auto& entry:labels){
                std::string value=generateExpression(entry.first->children[0]);
                std::string test=newTemp();
                out<<test<<" = "<<selector<<" == "<<value<<'\n';
                out<<"if "<<test<<" goto "<<entry.second<<'\n';
            }
            out<<"goto "<<defaultLabel<<'\n';
            breakLabels.push_back(end);
            for(Node* item:cases->children){
                if(item->kind=="case"){
                    for(const auto& entry:labels) if(entry.first==item) out<<entry.second<<":\n";
                    generateStatement(item->children[1]);
                } else { out<<defaultLabel<<":\n"; generateStatement(item->children[0]); }
            }
            breakLabels.pop_back();
            out<<end<<":\n";
            return;
        }

        if (node->kind == "break") {
            if(!breakLabels.empty()) out<<"goto "<<breakLabels.back()<<'\n';
            return;
        }

        // Scanner sc = new Scanner(System.in); only opens the input: no code
        if (node->kind == "scanner") {
            return;
        }

        std::cerr << "TAC warning: unsupported statement node: "
                  << node->kind << '\n';
    }

public:

    explicit TACGenerator(std::ostream& output)
        : out(output) {}

    void generate(Node* root) {

        tempCount = 0;
        labelCount = 0;
        breakLabels.clear();
        continueLabels.clear();
        inMethod = false;
        mainEndLabel.clear();

        generateStatement(root);
    }
};

// Public function
void generateTac(Node* root, std::ostream& output) {

    TACGenerator generator(output);

    generator.generate(root);
}
