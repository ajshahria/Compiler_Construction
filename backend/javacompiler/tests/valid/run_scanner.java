// flags: --run
// input: 41 1.25 true
import java.util.Scanner;

class ReadInput {
    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        int a = sc.nextInt();
        float b;
        b = sc.nextFloat();
        boolean ok = sc.nextBoolean();
        if (ok) {
            System.out.println(a + 1);
            System.out.println(b * 2);
        }
    }
}
