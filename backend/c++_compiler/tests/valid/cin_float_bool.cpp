// stdin: 7 2.5 1
#include <iostream>
using namespace std;
int main() {
    int n; double d; bool flag;
    cin >> n >> d >> flag;
    cout << n * 2 << " " << d * 2 << " " << flag << endl;
    cout << "n is " << (n > 5 ? 1 : 0) << endl;
}
