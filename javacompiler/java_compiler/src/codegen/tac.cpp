
#include "codegen/tac.h"

#include <iostream>
#include <ostream>
#include <string>

class TACGenerator {

private:
    std::ostream& out;

    int tempCount = 0;
    int labelCount = 0;

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

        // Literal or variable
        if (node->kind == "literal" ||
            node->kind == "id") {

            return node->text;
        }

        // Unary expression
        if (node->kind == "unary") {

            std::string operand =
                generateExpression(node->children[0]);

            std::string temp = newTemp();

            out << temp << " = "
                << node->text << operand << '\n';

            return temp;
        }

        // Binary expression
        if (node->kind == "binary") {

            // Short-circuit logical AND
            if (node->text == "&&") {

                std::string result = newTemp();
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

                out << result << " = 1\n";

                out << "goto " << endLabel << '\n';

                out << falseLabel << ":\n";
                out << result << " = 0\n";

                out << endLabel << ":\n";

                return result;
            }

            // Short-circuit logical OR
            if (node->text == "||") {

                std::string result = newTemp();
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

                out << result << " = 0\n";

                out << "goto " << endLabel << '\n';

                out << trueLabel << ":\n";
                out << result << " = 1\n";

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

                out << node->children[0]->text
                    << " = " << result << '\n';
            }

            return;
        }

        // Assignment
        if (node->kind == "assign") {

            std::string result =
                generateExpression(node->children[0]);

            out << node->text
                << " = " << result << '\n';

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

            generateStatement(node->children[1]);

            out << "goto " << startLabel << '\n';

            out << endLabel << ":\n";

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

        generateStatement(root);
    }
};

// Public function
void generateTac(Node* root, std::ostream& output) {

    TACGenerator generator(output);

    generator.generate(root);
}
