// Syntax errors: a missing ';' and an operator with no right operand.
// Error recovery reports both in one run.
int a;
int b;
a = 10
print a;
b = 20 + ;
if (a > b) {
    print a;
}
print b;
