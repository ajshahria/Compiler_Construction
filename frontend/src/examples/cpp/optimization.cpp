// Compare the TAC and Optimized TAC tabs: constant propagation and folding,
// and removal of the branch that can never run.
#include <iostream>
using namespace std;

int main() {
    int a = 4;
    int b = a * 2 + 1;
    int c = b - a;
    if (a > 100) {
        cout << 0 << endl;
    } else {
        cout << c * 10 << endl;
    }
    return 0;
}
