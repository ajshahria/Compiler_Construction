// Compare the TAC and Optimized TAC tabs: constant propagation and folding,
// and removal of the branch that can never run.
class Optimize {
    public static void main(String[] args) {
        int a = 4;
        int b = a * 2 + 1;
        int c = b - a;
        if (a > 100) {
            System.out.println(0);
        } else {
            System.out.println(c * 10);
        }
    }
}
