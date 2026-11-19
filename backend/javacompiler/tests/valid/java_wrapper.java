// The manual's sample written as a Java class with System.out.println.
class Sample {
    public static void main(String[] args) {
        int x = 10;
        int y = 0;
        boolean flag = true;
        while (x > 0) {
            y = y + x;
            x = x - 1;
        }
        if (flag == true) {
            System.out.println(y);
        } else {
            System.out.println(x);
        }
    }
}
