// flags: --allow-no-main
#include <iostream>
int a;
{
    int inner = 1;
    a = inner;
}
a = inner + 1;
