// Bonus arrays and methods: every rule the semantic analyzer checks.
class Errors {
    static int twice(int x) {
        return x * 2;
    }

    static void show(float v) {
        System.out.println(v);
    }

    static int noReturn(int x) {
        if (x > 0) {
            return 1;
        }
    }

    static boolean wrong() {
        return 5;
    }

    static void sum(int[] values) {
        return;
    }

    public static void main(String[] args) {
        int[] a = new int[3];
        int[] b = new float[2];
        int n = 4;
        int[] c = new int[n];
        float[] d = {1.5, true};
        a[1.5] = 2;
        n = a;
        a = 5;
        n[0] = 1;
        n = twice(1, 2);
        n = twice(false);
        n = show(1.0);
        n = missing(3);
        n = n(3);
        n = twice;
        n = d.length + a.length;
        continue;
        return 1;
    }
}
