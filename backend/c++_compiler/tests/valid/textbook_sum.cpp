// stdin: 3 4
#include <iostream>

int main() {
    int num1; // Declares an integer variable
    int num2;
    int sum;

    std::cout << "Enter the first number: ";
    std::cin >> num1; // Takes input from the user and stores it in num1

    std::cout << "Enter the second number: ";
    std::cin >> num2;

    // Calculates the sum
    sum = num1 + num2;

    // Displays the result
    std::cout << "The sum is: " << sum << "\n";

    return 0;
}
