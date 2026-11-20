class ManualSample {
    public static void main(String[] args) {
        // Sample program from section 5.5 of the lab manual, written exactly as in the manual.
        int x;
        int y;
        bool flag;

        x = 10;
        y = 0;
        flag = true;

        while (x > 0) {
            y = y + x;
            x = x - 1;
        }

        if (flag == true) {
            print y;
        } else {
            print x;
        }
    }
}
