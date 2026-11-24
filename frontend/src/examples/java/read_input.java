// Reads two numbers with a Scanner (the input box below holds them), then prints.
import java.util.Scanner;

class ReadInput {
    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        int count = sc.nextInt();
        float price = sc.nextFloat();
        System.out.println(count * price);
    }
}
