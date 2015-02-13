/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */

#include "csapp.h"

int fibonacci(int n) { 
    if (n <= 2) {
        return 1;
    } else {
        return fibonacci(n-1) + fibonacci(n-2);
    }
}

/* Adds two numbers and writes them out */
void adder13(int fd, char* args) {
    char *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;

    // TODO: Error handling
    p = strchr(args, '&');
    *p = '\0';
    strcpy(arg1, args);
    strcpy(arg2, p+1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);

    /* Make the response body */
    sprintf(content, "%sThe answer is: %d + fib(%d) = %d\r\n<p>", 
            content, n1, n2, n1+fibonacci(n2));
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /* Generate the HTTP response */
    printf("Content-length: %lu\r\n", strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    //printf("Content: %s\n", content);
    Rio_writen(fd, content, strlen(content));
    return;
}

