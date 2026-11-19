#include <iostream>
using namespace std;
int main() {
    int grid = 0;
    for (int r = 1; r <= 4; r++) {
        for (int c = 1; c <= 4; c++) {
            if (c > r) break;
            cout << r * c << "\t";
            grid += r * c;
        }
        cout << endl;
    }
    int k = 10;
    while (k > 0) {
        k -= 3;
        if (k == 4) continue;
        cout << k << " ";
    }
    cout << endl << "grid=" << grid << endl;
    bool flag = grid > 20 && !(grid == 100);
    cout << flag << endl;
    int day = 3;
    switch (day) {
        case 1: cout << "Mon"; break;
        case 3: cout << "Wed"; break;
        default: cout << "?";
    }
    cout << endl;
    return 0;
}
