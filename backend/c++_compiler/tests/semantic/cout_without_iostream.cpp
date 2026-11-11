// flags: --manual
// cout needs #include <iostream>  (checked by the semantic phase when the header rule is relaxed)
#include <vector>
int x = 5;
cout << x;
