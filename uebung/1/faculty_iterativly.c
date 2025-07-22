/**
 * @file faculty_iterativly.c
 * @brief Calculates the factorial of a number using an iterative approach.
 *
 * This program prompts the user to enter a non-negative integer, calculates
 * its factorial using a for-loop, and prints the result.
 */

#include <stdio.h>

/**
 * @brief Calculates the factorial of a non-negative integer iteratively.
 *
 * The factorial of n (denoted as n!) is the product of all positive integers
 * up to n. (e.g., 5! = 5 * 4 * 3 * 2 * 1 = 120).
 *
 * @param n The integer to calculate the factorial of. Should be non-negative.
 * @return The factorial of n. Returns 1 for n=0.
 */
int faculty(int n) {
    int result = 1; // Start with 1, as 0! is 1 and it's the multiplicative identity.
    // Loop from 2 up to n, multiplying the result by each number.
    for (int i = 2; i <= n; ++i) {
        result *= i; // Same as: result = result * i;
    }
    return result;
}

/**
 * @brief Main function to get user input and display the factorial.
 */
int main() {
    int num;

    // Prompt the user for input.
    printf("Enter a non-negative integer: ");
    // Read the integer from standard input.
    // scanf returns the number of items successfully read. We expect 1.
    if (scanf("%d", &num) != 1) {
        printf("Invalid input. Please enter an integer.\n");
        return 1; // Exit with an error code.
    }

    // Check if the number is negative, as factorial is not defined for them.
    if (num < 0) {
        printf("Factorial is not defined for negative numbers.\n");
    } else {
        // If the number is valid, call the faculty function and store the result.
        int result = faculty(num);
        // Print the final result.
        printf("Factorial of %d is %d\n", num, result);
    }

    return 0; // Exit successfully.
}
