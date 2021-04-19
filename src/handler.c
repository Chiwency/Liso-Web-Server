#include "../include/handler.h"
#include <time.h>

void serverLog(char *type, char *info)
{
    fprintf(stdout, "%s: %s\n", type, info);
}

void Send_Ressponse(SSL *client_context, char *errnum,
                    char *shortmsg, char *body)
{
    char buf[MAXLINE];
    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    SSL_write(client_context, buf, strlen(buf));
    sprintf(buf, "Connection: %s\r\n", "keep-alive");
    SSL_write(client_context, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n", 0);
    SSL_write(client_context, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    SSL_write(client_context, buf, strlen(buf));
}

void clienterror(SSL *client_context, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE];
    //    int n = 114 + strlen(cause) + strlen(errnum) + strlen(shortmsg) + strlen(longmsg);
    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    SSL_write(client_context, buf, strlen(buf));
    sprintf(buf, "Connection: %s\r\n", "keep-alive");
    SSL_write(client_context, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n", 0);
    SSL_write(client_context, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    SSL_write(client_context, buf, strlen(buf));

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
    connectionPool->lintenfd = fd;
    connectionPool->ssl_context = Init_SSL();
    FD_ZERO(&connectionPool->read_fds);
    FD_SET(fd, &connectionPool->read_fds);
    return connectionPool;
}

SSL_CTX *Init_SSL()
{
    SSL_CTX *ssl_context;
    SSL_load_error_strings();
    SSL_library_init();
    if ((ssl_context = SSL_CTX_new(TLS_server_method())) == NULL)
    {
        fprintf(stderr, "Error creating SSL context.\n");
        exit(1);
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_context, PRIVATE_KEY_PATH,
                                    SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(ssl_context);
        fprintf(stderr, "Error associating private key.\n");
        exit(1);
    }
    if (SSL_CTX_use_certificate_file(ssl_context, PUBLIC_KEY_PATH,
                                     SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(ssl_context);
        fprintf(stderr, "Error associating certificate.\n");
        exit(1);
    }
    return ssl_context;
}

int Init_Client(ConnectionPool *connPool)
{
    struct sockaddr_storage clientaddr;
    socklen_t clientlen;
    SSL *client_context;
    clientlen = sizeof(clientaddr);
    char hostname[MAXLINE], port[MAXLINE];

    client_context = NULL;
    int connfd = Accept(connPool->lintenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 1024)
    {
        serverLog("Error", "out of max connctions in parallel");
        close(connfd);
        return -1;
    }
    if ((client_context = SSL_new(connPool->ssl_context)) == NULL)
    {
        close(connfd);
        serverLog("Error", "1:Init SSL for client failed");
        return -1;
    }
    if (SSL_set_fd(client_context, connfd) == 0)
    {
        close(connfd);
        SSL_free(client_context);
        serverLog("Error", "2:Init SSL for client failed");
        return -1;
    }
    if (SSL_accept(client_context) <= 0)
    {
        close(connfd);
        SSL_free(client_context);
        serverLog("Error", "3:Init SSL for client failed");
        return -1;
    }

    connPool->client_fd[connfd] = (Client *)malloc(sizeof(Client));
    if (!connPool->client_fd[connfd])
    {
        serverLog("Error", "Out of Memory");
        clienterror(client_context, NULL, STATUS_SERVER_ERROR, "Server Error",
                    "Liso web server can't handdle your request, Plese wait");
        SSL_free(client_context);
        close(connfd);
        return -1;
    }
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    FD_SET(connfd, &connPool->read_fds);

    memset(connPool->client_fd[connfd]->buf, 0, MAXSIZE);
    connPool->client_fd[connfd]->request = NULL;
    connPool->client_fd[connfd]->body = NULL;
    connPool->client_fd[connfd]->fd = connfd;
    connPool->client_fd[connfd]->client_context = client_context;

    return 1;
}

void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGHUP:
        serverLog("Info", "SIGHUP GET");
        break;
    case SIGTERM:
        liso_shutdown();
        break;
    default:
        break;
    }
}

void liso_shutdown()
{
    serverLog("Info:", "Shutdown Server");
    exit(0);
}

int daemonize()
{
    int i, lfp, pid = fork();
    char str[256] = {0};
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    setsid();                      // 创建一个新的session
    freopen(LOGFILE, "w", stdout); // 输入输出重定向
    //关掉所有文件描述符
    // for (i = getdtablesize(); i >= 0; i--)
    //     close(i);

    i = open("/dev/null", O_RDWR);
    dup2(i, 2); // 忽略错误
    umask(027);

    lfp = open(LOCKFILE, O_RDWR | O_CREAT, 0640);

    if (lfp < 0)
        exit(EXIT_FAILURE);

    if (lockf(lfp, F_TLOCK, 0) < 0)
        exit(EXIT_SUCCESS);

    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); /* record pid to lockfile */

    signal(SIGCHLD, SIG_IGN); /* child terminate signal */

    signal(SIGHUP, signal_handler);  /* hangup signal */
    signal(SIGTERM, signal_handler); /* software termination signal from kill */

    write(1, "Successfully daemonized lisod process, pid %d\n", getpid());

    return EXIT_SUCCESS;
}

void Free_Client(ConnectionPool *connPool, int i)
{
    SSL_shutdown(connPool->client_fd[i]->client_context);
    close(connPool->client_fd[i]->fd);
    free(connPool->client_fd[i]->client_context);
    connPool->client_fd[i]->client_context = NULL;
    free(connPool->client_fd[i]);
    connPool->client_fd[i] = NULL;
    FD_CLR(i, &connPool->read_fds);
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
        clienterror(client->client_context, req->http_method, STATUS_NOT_IMPLIMENT, "Not Implemented",
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
        Send_Ressponse(client->client_context, STATUS_OK, "OK", NULL);
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
        clienterror(client->client_context, filename, STATUS_NOT_FOUND, "Not found",
                    "Liso couldn't find this file");
        return;
    }
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
        clienterror(client->client_context, filename, "403", "Forbidden",
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
    SSL_write(client->client_context, buf, strlen(buf));
    sprintf(buf, "Content-length: %lld\r\n", sbuf.st_size);
    SSL_write(client->client_context, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n", filetype);
    SSL_write(client->client_context, buf, strlen(buf));
    sprintf(buf, "Last-Modified: %s\r\n", timeChar);
    SSL_write(client->client_context, buf, strlen(buf));
    sprintf(buf, "Connection: %s\r\n\r\n", "keep-alive");
    SSL_write(client->client_context, buf, strlen(buf));
    if (!strcmp(method, "HEAD"))
    {
        return;
    }
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    SSL_write(client->client_context, srcp, sbuf.st_size);
    Munmap(srcp, sbuf.st_size);
    return;
}
