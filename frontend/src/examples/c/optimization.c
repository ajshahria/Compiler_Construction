// Compare the TAC and Optimized TAC tabs: constant propagation and folding,
// and removal of the branch that can never run.
int a;
int b;
int c;
a = 4;
b = a * 2 + 1;
c = b - a;
if (a > 100) {
    print 0;
} else {
    print c * 10;
}
