// Syntax errors: a missing ';' and an operator with no right operand.
// Error recovery reports both in one run.
class Syntax {
    public static void main(String[] args) {
        int a = 10
        System.out.println(a);
        int b = 20 + ;
        System.out.println(b);
    }
}
