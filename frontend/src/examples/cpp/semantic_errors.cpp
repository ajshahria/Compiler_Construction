// One example of every semantic rule in section 4.5 of the lab manual.
#include <iostream>
using namespace std;

int main() {
    int x;
    int x;                  // redeclaration in the same scope
    bool flag = 5 + 3.2;    // type mismatch
    y = 10;                 // undeclared variable
    {
        int inner = 1;
    }
    inner = 2;              // scope violation
    x = true;               // invalid assignment
    flag = x && 1;          // invalid expression
    return 0;
}
