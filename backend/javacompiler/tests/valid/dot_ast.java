// flags: --dot
// Bonus: the AST as a Graphviz graph (dot -Tpng ast.dot -o ast.png).
class Dot {
    static int twice(int x) {
        return x * 2;
    }

    public static void main(String[] args) {
        int[] a = {1, 2};
        System.out.println(twice(a[1]));
    }
}
