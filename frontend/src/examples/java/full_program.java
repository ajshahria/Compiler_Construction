// A full Java program: for, do-while, switch, ++ and System.out.println.
class Summary {
    public static void main(String[] args) {
        int sum = 0;
        for (int i = 1; i <= 5; i++) {
            sum = sum + i * i;
        }
        float average = sum / 5.0;
        boolean big = sum > 50;

        int countdown = 3;
        do {
            System.out.println(countdown);
            countdown--;
        } while (countdown > 0);

        switch (sum % 3) {
            case 0:
                System.out.println(0);
                break;
            default:
                System.out.println(sum % 3);
        }
        if (big) {
            System.out.println(average);
        }
    }
}
