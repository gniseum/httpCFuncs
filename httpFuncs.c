/**
 * 
 */

#include "httpFuncs.h"




void httpCmdHandler(int remote, char *buf, char *path) {
        char *error400 = "HTTP/1.1 400 Bad Request\r\n";
    char content[] = "Content-type: text/html\r\n";
    char endHead[] = "\r\n\r\n";
    char *get = "GET";
    char *token;

    /* For Date I looked at URL: https://stackoverflow.com/questions/7548759
     * /generate-a-date-string-in-http-response-date-format-in-c
     * AUTHOR: pmg DATE: 25 Sept 2011*/
    char timeBuff[50] = {'\0'};
    time_t curr = time(NULL);
    struct tm tm = *gmtime(&curr);
    strftime(timeBuff, sizeof(timeBuff), "%a, %d, %b, %Y %H:%M:%S %Z", &tm);
    strncat(timeBuff, "\r\n", 49 - strlen(timeBuff));
    // End of credited code

    // Struct used to determine reply
    request reply = {0};
    reply.content = content;
    reply.endHead = endHead;
    reply.date = timeBuff;

    // Check the HTTP command, base project only needs to support GET
    token = strtok(buf, " ");
    int strCmp = strncmp(token, get, strlen(get));
    if(strCmp != 0) {
        fprintf(stderr, "Bad/Unsupported Request %s\n", token);
        reply.flag = 400;
        reply.msg = error400;
        reply.file = NULL;
        httpSender(&reply, remote);
        /* Free and exit here due to how memory is allocated.
         * If free and exit are removed here there will be a block that is
         * not free unlike returning later in this function.*/
        free(path);
        exit(0);
    }
    char *error404 = "HTTP/1.1 404 Not Found\r\n";
    char *error403 = "HTTP/1.1 403 Forbidden response\r\n";
    char *ok200 = "HTTP/1.1 200 OK\r\n";
    char contentLen[50] = {'\0'};
    strncat(contentLen, "Content-length: ", 17);
    char *www = "/www";
    // For web browser scripts
    char *cgi = "/cgi-bin/"; 

    // Check the file requested
    token = strtok(NULL, " ");
    // Requests are not allowed to use .. in this program
    if(strstr(token, "/..") != NULL) {
        reply.flag = 404;
        reply.msg = error404;
        reply.file = strncat(path, token, PATH_MAX - strlen(path));
    }
    // cgi-bin check
    else if(strstr(token, cgi) != NULL) {
        reply.file = strncat(path, token, PATH_MAX - strlen(path));
        // Does it exist?
        if(!(fileExists(reply.file))) {
            reply.flag = 404;
            reply.msg = error404;
        }
        // cgi-bin must be executable
        else {
            bool execs = fileExecs(reply.file);
            // cgi is not executable
            reply.flag = 403;
            reply.msg = error403;
            if(execs) {
                // cgi is executable
                reply.flag = 200;
                reply.msg = NULL;
            }
        }
    }
    else {
        // Check for default /index.html
        if(strncmp(token, "/", 2) == 0) {
            token = "/index.html";
        }
        // Catch everything else as /www
        else {
            strncat(path, www, strlen(www));
        }
        reply.file = strncat(path, token, PATH_MAX - strlen(path));
        // Check if HTML exists
        if(fileExists(reply.file)) {
            // HTML not readable
            reply.flag = 403;
            reply.msg = error403;
            if(fileRead(reply.file)) {
                // Get the size of the file and strncat it
                int len = 0;
                if((len = fileSize(reply.file)) < 0) {
                    len = 0;
                }
                char lenStr[10] = {'\0'};
                snprintf(lenStr, 10, "%d", len);
                strncat(contentLen, lenStr, 10);
                strncat(contentLen, "\r\n", 3);
                reply.len = contentLen;
                // HTML readable
                reply.flag = 200;
                reply.msg = ok200;
            }
        }
        // Cannot find HTML file
        else {
            reply.flag = 404;
            reply.msg = error404;
        }
    }
    printf("File requested: %s Code: %d\n", reply.file, reply.flag);
    reply.file[strlen(reply.file)] = '\0';
    // Send the page
    httpSender(&reply, remote);
}

/* httpSender sends the page content to requester or an error*/
void httpSender(request *reply, int remote) {
    char *error500 = "HTTP/1.1 500 Internal Server Error\r\n";
    int nmemb;
    FILE *html = NULL;
    char *err2Prnt = NULL;

    // cgi-bin
    if(reply->flag == 200 && reply->msg == NULL) {
        html = popen(reply->file, "r");                    
    }
    // html
    else if(reply->flag == 200) {
        html = fopen(reply->file, "r");
    }
    else {
        err2Prnt = errorPage(reply->flag);
        send(remote, reply->msg, strlen(reply->msg), 0);
        send(remote, reply->date, strlen(reply->date), 0);
        send(remote, reply->content, strlen(reply->content), 0);
        send(remote, reply->endHead, strlen(reply->endHead), 0);
        send(remote, err2Prnt, strlen(err2Prnt), 0);
        shutdown(remote, SHUT_WR);
        free(err2Prnt);
        
        return;
    }

    // File could not be opened to be read
    if(!html) {
        err2Prnt = errorPage(500);
        send(remote, error500, strlen(error500), 0);
        send(remote, reply->date, strlen(reply->date), 0);
        send(remote, reply->content, strlen(reply->content), 0);
        send(remote, reply->endHead, strlen(reply->endHead), 0); 
        shutdown(remote, SHUT_WR);
        free(err2Prnt);

        return;
    }
    else {
        // cgi-bin will fail this check and be read directly
        if(reply->msg != NULL) {
            //send the header. 
            send(remote, reply->msg, strlen(reply->msg), 0);
            send(remote, reply->date, strlen(reply->date), 0);
            send(remote, reply->len, strlen(reply->len), 0);
            send(remote, reply->content, strlen(reply->content), 0);
            // End the replys
            send(remote, reply->endHead, strlen(reply->endHead), 0); 
        }
        char read[512] = {'\0'};
        // While there is stuff to read bring it in
        while((nmemb = fread(read, sizeof(char), 512, html)) > 0) {
            read[nmemb - 1] = '\0';
            send(remote, read, nmemb, 0);
        }
    }
    // Force a flush of the page. This worked unlike TCP_NODELAY
    shutdown(remote, SHUT_WR);
    if(html != NULL) {
        fclose(html); 
    }
}

/* Make the HTML error page*/
char * errorPage(int error) {
    char *httpPfx = "<!doctype html>\n<html>\n<head>\n"
                    "\t<title>Error!</title>\n"
                    "</head>\n<body>\n\t<h1>";
    char *httpEnd = "</h1>\n</body>\n</html>";
    char *err = NULL;
    // Switch allows the errors that are supported to grow
    switch(error) {
        case 500:
            err = "500 Internal Server Error";
            break;
        case 404:
            err = "404 Not Found\r\n";
            break;
        case 403:
            err = "403 Forbidden response\r\n";
            break;
        case 400:
            err = "400 Bad Request\r\n";
            break;
        default:
            // Catch for unsupported errors
            err = "502 Unkown Error\r\n";
    }
    // Verbose attempt at secure strncat
    int totalLen = strlen(httpPfx) + strlen(httpEnd) + strlen(err);
    char *errorStr = calloc(totalLen + 4, 1);
    strncat(errorStr, httpPfx, totalLen - 1);
    strncat(errorStr, err, totalLen - 1);
    strncat(errorStr, httpEnd, totalLen - 1);

    return errorStr;
}