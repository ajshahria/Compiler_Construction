// flags: --opt
// Optimizer: constants are propagated and folded, the if with a known
// condition keeps only the branch that runs, and unused temporaries go away.
class Folding {
    public static void main(String[] args) {
        int a = 5;
        int b = a * 2 + 3;
        int c = b - a;
        boolean big = c > 5;
        if (big) {
            System.out.println(c);
        } else {
            System.out.println(a);
        }
    }
}
