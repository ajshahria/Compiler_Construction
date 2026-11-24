// A full C program: every data type, nested blocks, if-else, while and printf.
#include <stdio.h>

int main() {
    int n = 5;
    int factorial = 1;
    float average = 0.0;
    bool even;

    while (n > 0) {
        factorial = factorial * n;
        n = n - 1;
    }
    average = factorial / 4.0;
    even = factorial % 2 == 0;

    if (even && average > 10.0) {
        int bonus = 3;
        printf("factorial = %d, average = %.2f\n", factorial, average + bonus);
    } else {
        printf("small or odd\n");
    }
    return 0;
}
