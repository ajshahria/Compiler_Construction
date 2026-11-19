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
 * label of the innermost loop or switch.
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

        // Program and block
        if (node->kind == "program" ||
            node->kind == "block") {

            for (Node* child : node->children) {
                generateStatement(child);
            }

            return;
        }

        // Variable declaration
        if (node->kind == "decl") {

            if (node->children.size() > 1) {

                std::string result =
                    generateExpression(node->children[1]);

                out << node->tacName
                    << " = " << result << '\n';
            }

            return;
        }

        // Assignment
        if (node->kind == "assign") {

            std::string result =
                generateExpression(node->children[0]);

            out << node->tacName
                << " = " << result << '\n';

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
            generateStatement(node->children[1]);
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
            breakLabels.push_back(end);
            generateStatement(node->children[3]);
            breakLabels.pop_back();
            if(node->children[2]) generateStatement(node->children[2]);
            out<<"goto "<<start<<'\n'<<end<<":\n";
            return;
        }

        if (node->kind == "do_while") {
            std::string start=newLabel(), end=newLabel();
            out<<start<<":\n";
            breakLabels.push_back(end);
            generateStatement(node->children[0]);
            breakLabels.pop_back();
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

        generateStatement(root);
    }
};

// Public function
void generateTac(Node* root, std::ostream& output) {

    TACGenerator generator(output);

    generator.generate(root);
}
