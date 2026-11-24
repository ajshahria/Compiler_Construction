// Lexical errors: an identifier that starts with a digit and an invalid character.
// Every one is reported with its line number.
#include <iostream>
using namespace std;

int main() {
    int total;
    int 2count;
    total$ = 5 + 3;
    cout << total << endl;
    return 0;
}
