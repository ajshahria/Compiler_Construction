// C++ style extras: for loop, ++/--, compound assignment, cout
#include <iostream>
using namespace std;

int main() {
    int sum = 0;
    for (int i = 1; i <= 5; i++) {
        sum += i;
    }
    cout << sum << endl;
    int k = 10;
    k -= 3;
    k *= 2;
    k--;
    cout << k << endl;
    return 0;
}
