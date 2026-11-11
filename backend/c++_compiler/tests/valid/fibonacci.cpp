#include <iostream>
using namespace std;
int main() {
    int n = 20, a = 0, b = 1;
    cout << "Fibonacci: ";
    for (int i = 0; i < n; ++i) {
        cout << a << " ";
        int t = a + b;
        a = b;
        b = t;
    }
    cout << endl;
    return 0;
}
