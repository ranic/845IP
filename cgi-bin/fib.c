/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "stdio.h"
int fibonacci(int n) { 
    if (n <= 2) {
        return 1;
    } else {
        return fibonacci(n-1) + fibonacci(n-2);
    }
}


int main(void) {
    printf("Result: %d\n", fibonacci(46));
    return 0;
}
