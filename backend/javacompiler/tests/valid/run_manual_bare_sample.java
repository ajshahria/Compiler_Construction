// flags: --manual --run
// The sample program of manual section 5.5 exactly as written (no class),
// executed: y = 10 + 9 + ... + 1.
int x;
int y;
bool flag;

x = 10;
y = 0;
flag = true;

while (x > 0) {
    y = y + x;
    x = x - 1;
}

if (flag == true) {
    print y;
} else {
    print x;
}
