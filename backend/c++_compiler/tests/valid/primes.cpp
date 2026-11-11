#include <iostream>
using namespace std;
int main() {
    int count = 0;
    for (int num = 2; num <= 50; num++) {
        bool prime = true;
        for (int d = 2; d * d <= num; d++) {
            if (num % d == 0) { prime = false; break; }
        }
        if (prime) { cout << num << ' '; count++; }
    }
    cout << "\nprimes: " << count << '\n';
    return 0;
}
