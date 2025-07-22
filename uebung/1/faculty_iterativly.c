/**
 * @file faculty_iterativly.c
 * @brief Calculates the factorial of a number using an iterative approach.
 *
 * This program prompts the user to enter a non-negative integer, calculates
 * its factorial using a for-loop, and prints the result.
 */

#include <stdio.h>
#include <stdlib.h>

long faculty(int n) {
    long ret = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i; // Same as: result = result * i;
    }
    return result;
}

int get_number_from_user() {
    int num;
    printf("Enter a non-negative integer: ");
    // Read the integer from standard input.
    // scanf returns the number of items successfully read. We expect 1.
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

    // Check if the number is negative, as factorial is not defined for them.
    if (num < 0) {
        printf("Factorial is not defined for negative numbers.\n");
    } else {
        long result = faculty(num);
        printf("Factorial of %d is %ld\n", num, result);
    }

    return 0; // Exit successfully.
}
