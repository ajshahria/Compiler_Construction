// standard headers, using namespace, cout with endl
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
using namespace std;

int main() {
    int total = 0;
    for (int i = 1; i <= 4; i++) {
        total += i * i;
    }
    cout << total << endl;
    std::cout << total + 1 << std::endl;
    return 0;
}
