// flags: --run
// Bonus: arrays, static methods with parameters and return values,
// recursion, continue and a.length.
public class ArraysAndMethods {
    static int factorial(int n) {
        if (n <= 1) {
            return 1;
        }
        return n * factorial(n - 1);
    }

    static int fib(int n) {
        if (n < 2) {
            return n;
        }
        return fib(n - 1) + fib(n - 2);
    }

    static float half(int x) {
        return x / 2.0;
    }

    static boolean isEven(int n) {
        return n % 2 == 0;
    }

    static void show(int value) {
        System.out.println(value);
    }

    public static void main(String[] args) {
        int[] data = {5, 2, 9, 1, 7, 3};
        int[] squares = new int[5];

        System.out.println(factorial(5));
        System.out.println(fib(10));
        System.out.println(half(7));

        // bubble sort
        for (int i = 0; i < data.length - 1; i++) {
            for (int j = 0; j < data.length - 1 - i; j++) {
                if (data[j] > data[j + 1]) {
                    int swap = data[j];
                    data[j] = data[j + 1];
                    data[j + 1] = swap;
                }
            }
        }
        for (int i = 0; i < data.length; i++) {
            show(data[i]);
        }

        int evens = 0;
        for (int i = 0; i < squares.length; i++) {
            squares[i] = i * i;
            if (!isEven(squares[i])) {
                continue;
            }
            evens++;
        }
        squares[4]++;
        System.out.println(squares[4]);
        System.out.println(evens);
    }
}
