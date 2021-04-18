#include "../include/handler.h"
#include <time.h>

void serverLog(char *type, char *info)
{
    printf("%s: %s\n", type, info);
}

void read_requesthdrs(rio_t *rp, char *buff)
{
    char buf[MAXLINE];
    memset(buf, 0, MAXLINE);
    Rio_readlineb(rp, buf, MAXLINE);
    strcat(buff, buf);
    while (strcmp(buf, "\r\n"))
    { //line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        strcat(buff, buf);
    }
    return;
}
void Send_Ressponse(int fd, char *errnum,
                    char *shortmsg, char *body)
{
    char buf[MAXLINE];
    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Connection: %s\r\n", "keep-alive");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n", 0);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE];
    //    int n = 114 + strlen(cause) + strlen(errnum) + strlen(shortmsg) + strlen(longmsg);
    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Connection: %s\r\n", "keep-alive");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n", 0);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    // /* Print the HTTP response body */
    // sprintf(buf, "<html><title>Liso Server Error</title>");
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "<body bgcolor="
    //              "ffffff"
    //              ">\r\n");
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "<hr><em>The Liso Web server</em>\r\n");
    // Rio_writen(fd, buf, strlen(buf));
}

ConnectionPool *Init_Connection_Pool(int fd)
{
    ConnectionPool *connectionPool = (ConnectionPool *)malloc(sizeof(ConnectionPool));
    FD_ZERO(&connectionPool->read_fds);
    FD_SET(fd, &connectionPool->read_fds);
    return connectionPool;
}

void Free_Request(Client *client)
{
    free(client->request->headers);
    //   memset(client->request->http_uri, 0, 4096);
    client->request->headers = NULL;
    free(client->request);
    client->request = NULL;
    free(client->body);
    client->body = NULL;
    return;
}

int Handle_Request(Client *client)
{
    Request *req = client->request;
    int fd = client->fd;
    if (!client->body)
    {
        int len = 0;
        for (int i = 0; i < req->header_capacity; i++)
        {
            Request_header *header = &req->headers[i];
            if (!strcmp(header->header_name, "Content-Length"))
            {
                sscanf(header->header_value, "%d", &len);
                break;
            }
        }
        client->body = (char *)malloc(sizeof(char) * (len + 1));
        client->body[0] = '\0';
    }
    int n = sizeof(*client->body) - strlen(client->body) - 1;
    if (n > 0)
    {
        if (strlen(client->buf) >= n)
        {
            strncat(client->body, client->buf, n);
            char *p = client->buf + n;
            char buf[MAXSIZE];
            strcpy(buf, p);
            strcpy(client->buf, buf);
        }
        else
        {
            strcat(client->body, client->buf);
            memset(client->buf, 0, MAXSIZE);
            return 1; // 1表示要回去继续等待读数据
        }
    }

    if (strcmp(req->http_method, "GET") &&
        strcmp(req->http_method, "HEAD") &&
        strcmp(req->http_method, "POST"))
    {
        clienterror(fd, req->http_method, STATUS_NOT_IMPLIMENT, "Not Implemented",
                    "Liso web server does not implement this method");
        // 处理完每个请求后要把body释放
        Free_Request(client);
        return 0;
    }
    Serve_Request(client);
    Free_Request(client);
    return 0;
}

void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg") || strstr(filename, ".jpeg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

void Serve_Request(Client *client)
{
    int fd = client->fd;
    char *method = client->request->http_method;
    if (!strcmp(method, "POST"))
    {
        Send_Ressponse(client->fd, STATUS_OK, "OK", NULL);
        return;
    }
    char filename[500];
    memset(filename, 0, 500);
    struct stat sbuf;
    //strcpy(filename, "../");
    char *p = strchr(client->request->http_uri, '/');
    strcat(filename, p + 1);
    if (stat(filename, &sbuf) < 0)
    {
        clienterror(fd, filename, STATUS_NOT_FOUND, "Not found",
                    "Liso couldn't find this file");
        return;
    }
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file");
        return;
    }

    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF], timeChar[100];
    time_t now = time(&sbuf.st_mtimespec.tv_sec);

    struct tm tm = *gmtime(&now);
    strftime(timeChar, sizeof(timeChar), "%a, %d %b %Y %H:%M:%S %Z", &tm);

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %lld\r\n", sbuf.st_size);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Last-Modified: %s\r\n", timeChar);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Connection: %s\r\n\r\n", "keep-alive");
    Rio_writen(fd, buf, strlen(buf));
    if (!strcmp(method, "HEAD"))
    {
        return;
    }
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, sbuf.st_size);
    Munmap(srcp, sbuf.st_size);
    return;
}
