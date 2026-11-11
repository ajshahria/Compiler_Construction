#include <iostream>
using namespace std;

int main() {
    double c = -10;
    for (int row = 0; row < 4; row++) {
        double f = c * 9 / 5 + 32;
        cout << c << "C = " << f << "F" << endl;
        c += 15.5;
    }
    int steps = 0, n = 27;
    do {
        if (n % 2 == 0) n /= 2; else n = 3 * n + 1;
        steps++;
    } while (n != 1);
    cout << "collatz steps: " << steps << endl;
    return 0;
}
