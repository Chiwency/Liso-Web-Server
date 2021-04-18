#include <stdio.h>
#include <string.h>

int main ()
{
    char str1[500];
    strcpy(str1, "<html><title>Liso Server Error</title><body bgcolor=ffffff>\r\n%s: %s\r\n<p>%s: %s\r\n<hr><em>The Liso Web server</em>\r\n\0");

    printf("%lu ++", strlen(str1));
    return 0;
}