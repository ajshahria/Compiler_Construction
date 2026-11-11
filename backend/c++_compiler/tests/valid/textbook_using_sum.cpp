// stdin: 3 4
#include <iostream>
using namespace std;

int main() {
    int num1, num2, sum;

    cout << "Enter first number: ";
    cin >> num1; // Directly uses cin

    cout << "Enter second number: ";
    cin >> num2;

    sum = num1 + num2;

    // Notice how clean it is to chain outputs together
    cout << "The sum of " << num1 << " and " << num2 << " is " << sum << endl;

    return 0;
}
