/**
 * @file faculty_recusive.c
 * @brief Calculates the factorial of a number using a recursive approach.
 *
 * This program prompts the user to enter a non-negative integer, calculates
 * its factorial using a recursive function call, and prints the result.
 */

#include <stdio.h>

/**
 * @brief Calculates the factorial of a non-negative integer recursively.
 *
 * The function calls itself with a decreasing value until it reaches the
 * base case (n <= 1).
 *
 * @param n The integer to calculate the factorial of. Should be non-negative.
 * @return The factorial of n.
 */
int faculty(int n) {
    // Base case: The factorial of 0 or 1 is 1.
    // This condition stops the recursion.
    if (n <= 1) {
        return 1;
    }
    // Recursive step: The factorial of n is n multiplied by the factorial of (n-1).
    // The function calls itself with a smaller number.
    return n * faculty(n - 1);
}

/**
 * @brief Main function to get user input and display the factorial.
 */
int main() {
    int num;

    // Prompt the user for input.
    printf("Enter a non-negative integer: ");
    // Read the integer from standard input.
    if (scanf("%d", &num) != 1) {
        printf("Invalid input. Please enter an integer.\n");
        return 1; // Exit with an error code.
    }

    // Check if the number is negative.
    if (num < 0) {
        printf("Factorial is not defined for negative numbers.\n");
    } else {
        // If the number is valid, call the recursive function.
        int result = faculty(num);
        // Print the final result.
        printf("Factorial of %d is %d\n", num, result);
    }

    return 0; // Exit successfully.
}
