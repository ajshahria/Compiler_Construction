// flags: --run
// Bonus: in the lab manual's bare form, static methods can be written among
// the statements. A method sees only its parameters and its own variables.
static int square(int x) {
    return x * x;
}

int total = 0;
int i = 1;
while (i <= 3) {
    total = total + square(i);
    i++;
}
print total;

static boolean positive(float v) {
    return v > 0;
}

print positive(-2.5);
