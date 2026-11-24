// A full C++ program: for, switch, ++, +=, nested blocks and cout.
#include <iostream>
using namespace std;

int main() {
    int sum = 0;
    for (int i = 1; i <= 5; i++) {
        sum += i * i;
    }
    float average = sum / 5.0;
    bool big = sum > 50;

    switch (sum % 3) {
        case 0: cout << "divisible by 3" << endl; break;
        default: cout << "not divisible by 3" << endl;
    }
    if (big) {
        cout << "sum = " << sum << ", average = " << average << endl;
    }
    return 0;
}
