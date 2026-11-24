// Reads two numbers with cin (the input box below holds them), then prints.
#include <iostream>
using namespace std;

int main() {
    int count;
    float price;
    cin >> count >> price;
    cout << count << " items x " << price << " = " << count * price << endl;
    return 0;
}
