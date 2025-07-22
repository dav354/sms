#include <stdio.h>
#include <stdlib.h>

long faculty(int n) {
    if (n <= 1)
        return 1;
    return (long)n * faculty(n - 1);
}

int get_number_from_user() {
    int num;
    printf("Enter a non-negative integer: ");
    if (scanf("%d", &num) != 1) {
        printf("Invalid input. Please enter an integer.\n");
        exit(1);
    }
    return num;
}

int main(int argc, char *argv[]) {
    int num;

    if (argc > 1) {
        num = atoi(argv[1]);
    } else {
        num = get_number_from_user();
    }

    if (num < 0) {
        printf("Factorial is not defined for negative numbers.\n");
    } else {
        long result = faculty(num);
        printf("Factorial of %d is %ld\n", num, result);
    }

    return 0;
}
