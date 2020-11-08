#ifndef MY_HTTP_FUNCS
#define MY_HTTP_FUNCS

#include <stdbool.h>
#include <limits.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//struct to handle the request information
typedef struct request{
    char *file;
    char *msg;
    char *content;
    char *date;
    char *len;
    char *endHead;
    int flag;  
} request;

void httpCmdHandler(int remote, char *buf, char *path);
void httpSender(request *action, int remote);
char *errorPage(int error);

#endif