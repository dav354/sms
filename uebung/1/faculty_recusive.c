#include <stdio.h>

int faculty(int n) {
    if (n <= 1)
        return 1;
    return n * faculty(n - 1);
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
