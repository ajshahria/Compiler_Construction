#include <iostream>
using namespace std;

int main() {
    int a = 5, b = 12, c = 13;
    bool right = a * a + b * b == c * c;
    bool equilateral = (a == b) && (b == c);
    cout << "right: " << right << " equilateral: " << equilateral << endl;
    int total = 0;
    for (int i = 1; i <= 20; i++) {
        switch (i % 4) {
            case 0: continue;
            case 1: total += i; break;
            default: total -= 1;
        }
        if (total > 30) break;
    }
    cout << "total = " << total << endl;
    return 0;
}
