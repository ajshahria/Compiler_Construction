// flags: --run
// A valid program that fails while running: the output printed before the
// error is kept, then the run stops with a runtime error (exit status 1).
class DivisionByZero {
    public static void main(String[] args) {
        int a = 5;
        int z = 0;
        System.out.println(a);
        System.out.println(a / z);
        System.out.println(a);
    }
}
