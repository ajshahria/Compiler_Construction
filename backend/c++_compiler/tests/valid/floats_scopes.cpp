// flags: --allow-no-main
#include <iostream>
// float arithmetic, mixed int/float, nested scopes and shadowing
int n = 3;
float f = 2.5;
float g = f * n + 1;
bool big = g > 8.0;
print g;
print big;
{
    int n = 7;       // shadows the outer n
    print n % 4;
}
print n;
print -f;
print !big || false;
