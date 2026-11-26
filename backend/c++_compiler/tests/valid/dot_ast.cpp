// flags: --dot
// Bonus: the AST as a Graphviz graph (dot -Tpng ast.dot -o ast.png).
#include <iostream>
using namespace std;
int twice(int x) {
    return x * 2;
}
int main() {
    int v[2] = {3, 4};
    cout << twice(v[1]) << endl;
    return 0;
}
