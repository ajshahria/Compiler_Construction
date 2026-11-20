// flags: --run
// int/float/boolean arithmetic, int-to-float assignment, ||/&& with
// short-circuit jumps, for, switch and do-while, executed.
class Stress {
    public static void main(String[] args) {
        float f = 2;
        int i = 7;
        f = f / 4 + 1.5;
        boolean b = !(i > 3) || false;
        System.out.println(-i % 3);
        System.out.println(f);
        System.out.println(b);
        int s = 0;
        for (int k = 0; k < 5; k++) {
            if (k % 2 == 0) {
                s = s + k * k;
            } else {
                s = s - 1;
            }
        }
        System.out.println(s);
        switch (i) {
            case 1: System.out.println(10); break;
            case 7: System.out.println(70); break;
            default: System.out.println(0);
        }
        int n = 0;
        do {
            n = n + 3;
        } while (n < 10);
        print n;
        boolean c = i > 5 && f < 2.0;
        System.out.println(c);
        f = i;
        System.out.println(f * 0.5);
    }
}
