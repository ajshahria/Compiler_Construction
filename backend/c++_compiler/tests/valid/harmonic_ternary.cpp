#include <iostream>
#include <cmath>
using namespace std;
int main() {
    const int N = 5;
    double total = 0.0;
    for (int i = 1; i <= N; i++) total += 1.0 / i;
    cout << "H5 = " << total << endl;
    int x = 17;
    cout << (x % 2 == 0 ? 0 : 1) << endl;
    return 0;
}
