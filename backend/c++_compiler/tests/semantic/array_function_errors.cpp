// Bonus arrays and functions: every rule the semantic analyzer checks.
#include <iostream>
using namespace std;

int a[3] = {1, 2, 3, 4};
float b[2];
int n;

int twice(int x) {
    return x * 2;
}

void show(float v) {
    cout << v << endl;
}

int noValue() {
    return;
}

void wrong() {
    return 1;
}

int main() {
    a[1.5] = 2;
    n = a;
    a = 5;
    n[0] = 1;
    n = b[0];
    n = twice(1, 2);
    n = twice(true);
    n = show(1.0);
    n = missing(3);
    n = n(3);
    n = twice;
    cin >> a;
    show(2);
    return 0;
}
