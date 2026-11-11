// flags: --allow-no-main
#include <iostream>
// nested while/if-else, dangling else and logical operators
int i = 0;
int evens = 0;
while (i < 10) {
    if (i % 2 == 0)
        if (i > 4) evens = evens + 10; else evens = evens + 1;
    i = i + 1;
}
print evens;
bool ok = (evens > 5 && !(evens == 0)) || false;
print ok;
