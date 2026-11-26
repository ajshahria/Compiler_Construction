// Bonus: arrays, functions with parameters, return values and recursion,
// a global array shared by functions, and cin into an array element.
// stdin: 4 2.5
#include <iostream>
using namespace std;

int data[6] = {5, 2, 9, 1, 7, 3};

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

float average(int count) {
    int total = 0;
    for (int i = 0; i < count; i++) total += data[i];
    return total / 1.0 / count;
}

void sortData(int count) {
    for (int i = 0; i < count - 1; i++)
        for (int j = 0; j < count - 1 - i; j++)
            if (data[j] > data[j + 1]) {
                int swap = data[j];
                data[j] = data[j + 1];
                data[j + 1] = swap;
            }
}

float scale(float x, int times) {
    return x * times;
}

int main() {
    cout << "5! = " << factorial(5) << endl;
    cout << "fib(10) = " << fib(10) << endl;
    cout << "average = " << average(6) << endl;
    sortData(6);
    for (int i = 0; i < 6; i++) cout << data[i] << " ";
    cout << endl;
    int squares[5];
    for (int i = 0; i < 5; i++) squares[i] = i * i;
    squares[4]++;
    squares[1] += 10;
    cout << squares[4] << " " << squares[1] << endl;
    float input[2];
    cin >> squares[0] >> input[1];
    cout << squares[0] + 1 << " " << scale(input[1], 2) << " " << scale(3, 2) << endl;
    return 0;
}
