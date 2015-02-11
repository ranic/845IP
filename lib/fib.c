#include "csapp.h"
/* Adds two numbers and writes them out */
void fib(int fd, char* args) {
    char *p;
    char content[MAXLINE];
    int n=0;

    // TODO: Error handling
    n = atoi(args);

    /* Make the response body */
    sprintf(content, "%sThe answer is: fib(%d) = %d\r\n<p>", 
            content, n, fibonacci(n));
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /* Generate the HTTP response */
    printf("Content-length: %lu\r\n", strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    write(fd, content, strlen(content));
    return;
}

int fibonacci(int n) { 
    if (n <= 2) {
        return 1;
    } else {
        return fibonacci(n-1) + fibonacci(n-2);
    }
}
