// stdin: 2
#include <iostream>
using namespace std;

int main() {
    int a, b = 3;
    cin >> a;
    cout << "Sum: " << a + b << endl;
    if (a > b) cout << "greater\n"; else cout << "not greater\n";

    for (int i = 0; i < 6; i++) {
        if (i == 2) continue;
        if (i == 4) break;
        cout << i << " ";
    }
    cout << endl;

    int n = 5;
    long long f = 1;
    do {
        f *= n;
        n--;
    } while (n > 0);
    cout << "5! = " << f << endl;

    switch (a) {
        case 1: cout << "one"; break;
        case 2:
        case 3: cout << "two or three";
        default: cout << " (fell through)";
    }
    cout << endl;

    double avg = (a + b) / 2.0;
    cout << "avg " << avg << endl;
    cout << "ok? " << (avg > 2) << endl;
    return 0;
}
