// Scanner errors: missing import, a read of the wrong type, an undeclared
// Scanner, a variable used as a Scanner, and a read inside an expression.
class ScannerErrors {
    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        int a = sc.nextFloat();
        float f;
        f = sc.nextInt();
        int b = input.nextInt();
        int c = a.nextInt();
        int d = sc.nextInt() + 1;
        Scanner sc = new Scanner(System.in);
    }
}
