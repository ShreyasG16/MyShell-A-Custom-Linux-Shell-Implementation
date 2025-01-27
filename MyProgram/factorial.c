#include <stdio.h>
#include <stdlib.h>

// Function to calculate factorial
long factorial(int n) {
    if (n == 0) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main(int argc, char *argv[]) {
    int num;
    long fact;

    // Check if a command-line argument is provided
    if (argc == 2) {
        num = atoi(argv[1]); // Convert the argument to an integer
    } else {
        // Prompt the user to enter a number
        printf("Enter a number: ");
        scanf("%d", &num);
    }

    if (num < 0) {
        printf("Factorial is not defined for negative numbers.\n");
        return 1;
    }

    fact = factorial(num); // Calculate factorial
    printf("Factorial of %d is %ld\n", num, fact);
    return 0;
}
