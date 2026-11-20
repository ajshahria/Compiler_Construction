class NestedScopes {
    public static void main(String[] args) {
        /* Nested blocks, shadowing, logical operators,
           float arithmetic and if without else. */
        int n = 7;
        float total = 0.0;
        bool done = false;

        while (!done && n > 0) {
            int square = n * n;
            if (square % 2 == 0 || n == 1) {
                float half = square / 2.0;
                total = total + half;
            } else {
                int n = 100;        // shadows the outer n inside this block
                print n;
            }
            n = n - 1;
            if (n <= 0) done = true;
        }

        print total;
        print -n;
    }
}
