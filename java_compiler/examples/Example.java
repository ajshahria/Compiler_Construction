class Example {
    public static void main(String[] args) {
        int x = 5;
        int total = 0;
        while (x > 0) {
            total = total + x;
            x = x - 1;
        }
        if (total == 15) {
            System.out.println(total);
        } else {
            System.out.println(0);
        }
    }
}
