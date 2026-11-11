#include <iostream>
using namespace std;

int main() {
    int a = 84, b = 36;
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }
    cout << "GCD = " << a << endl;

    int num = 12321, rev = 0, copy = num;
    while (copy > 0) {
        rev = rev * 10 + copy % 10;
        copy /= 10;
    }
    cout << num << (rev == num ? " is" : " is not") << " a palindrome" << endl;
    return 0;
}
