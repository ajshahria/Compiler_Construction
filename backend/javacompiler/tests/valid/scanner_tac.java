// Each Scanner read becomes one 'read' instruction; nextDouble reads a float.
import java.util.*;

class Sum {
    public static void main(String[] args) {
        Scanner in = new Scanner(System.in);
        int n = in.nextInt();
        float total = 0.0;
        for (int i = 0; i < n; i++) {
            float x = in.nextDouble();
            total = total + x;
        }
        System.out.println(total);
    }
}
