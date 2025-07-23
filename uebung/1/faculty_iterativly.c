#include <stdio.h>

int faculty(int n) {
    int ret = 1;
    for (int i = 2; i <= n; ++i) {
        ret *= i;
    }
    return ret;
}


int main() {
    int num;

    printf("Enter a non-negative integer: ");
    if (scanf("%d", &num) != 1) {
        printf("Invalid input. Please enter an integer.\n");
        return 1;
    }

    if (num < 0) {
        printf("Factorial is not defined for negative numbers.\n");
    } else {
        int result = faculty(num);
        printf("Factorial of %d is %d\n", num, result);
    }

    return 0;
}
