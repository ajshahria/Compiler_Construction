// stdin: -5
#include <iostream>
int main() {
    int number;
    std::cout << "Enter an integer: ";
    std::cin >> number;
    // Checks the condition
    if (number > 0) {
        std::cout << "You entered a positive number.\n";
    } else if (number < 0) {
        std::cout << "You entered a negative number.\n";
    } else {
        std::cout << "You entered zero.\n";
    }
    return 0;
}
