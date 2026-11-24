// Reads two numbers with scanf (the input box below holds them), then prints.
#include <stdio.h>

int main() {
    int count;
    float price;
    scanf("%d %f", &count, &price);
    printf("%d items x %.2f = %.2f\n", count, price, count * price);
    return 0;
}
