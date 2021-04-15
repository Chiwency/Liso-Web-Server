/* C declarations used in actions */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/handler.h"

int handleClient(Client *client);

int main(int argc, char **argv)
{
    struct timeval tv;
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    fd_set read_fd_set;
    ConnectionPool *connPool;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]);
    connPool = Init_Connection_Pool(listenfd);
    int opend = 0, closed = 0;
    while (1)
    {
        // select()会原地修改fd_set的值，所以每次调用要重新初始化
        read_fd_set = connPool->read_fds;
        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, &tv) < 0)
        {
            serverLog("Error", "Select Error in main");
            continue;
        }
        for (int i = 3; i < FD_SETSIZE; ++i)
        {
            if (FD_ISSET(i, &read_fd_set))
            {
                if (i == listenfd)
                {
                    clientlen = sizeof(clientaddr);
                    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
                    if (connfd >= 1024)
                    {
                        serverLog("Error", "out of max connctions in parallel");
                        close(connfd);
                        continue;
                    }
                    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                                port, MAXLINE, 0);
                    printf("Accepted connection from (%s, %s)\n", hostname, port);
                    opend++;
                    printf("open:%d\n", opend);
                    FD_SET(connfd, &connPool->read_fds);
                }
                else
                {
                    if (!connPool->client_fd[i])
                    {
                        connPool->client_fd[i] = (Client *)malloc(sizeof(Client));
                        if (!connPool->client_fd[i])
                        {
                            serverLog("Error", "Out of Memory");
                            clienterror(i, NULL, STATUS_SERVER_ERROR, "Server Error",
                                        "Liso web server can't handdle your request, Plese wait");
                            close(i);
                            continue;
                        }
                        memset(connPool->client_fd[i]->buf, 0, MAXSIZE);
                        connPool->client_fd[i]->request = NULL;
                        connPool->client_fd[i]->body = NULL;
                        connPool->client_fd[i]->fd = i;
                    }
                    if (handleClient(connPool->client_fd[i]) == 0)
                    {
                        close(i);
                        closed++;
                        connPool->client_fd[i] = NULL;
                        printf("close:%d\n", closed);
                        FD_CLR(i, &connPool->read_fds);
                    }
                }
            }
        }
    }
    return 0;
}

int handleClient(Client *client)
{
    char buf[MAXSIZE];
    memset(buf, 0, MAXSIZE);
    int n = read(client->fd, buf, MAXSIZE);
    if (!client->request)
    {
        char *p = NULL;
        strcat(client->buf, buf);
        if (!(p = strstr(client->buf, "\r\n\r\n")))
        {
            if (n <= 0)
            {
                clienterror(client->fd, NULL, STATUS_BAD_REQUEST, "Bad Request",
                            "Bad Request, Please try again");
                return 0;
            }
            return 1;
        }
        p += 3;
        *p = '\0';
        strcpy(buf, client->buf);
        strcat(buf, "\n");
        client->request = parse(buf, strlen(buf), 0);
        if (client->request == NULL)
        {
            serverLog("Error", "parse http error in handleClient");
            clienterror(client->fd, NULL, STATUS_BAD_REQUEST, "Bad Request",
                        "Bad Request, Please try again");
            return 0;
        }
        p++;
        strcpy(buf, p);
        strcpy(client->buf, buf);
    }
    if (Parse_Header(client))
    {
        return 1;
    }
    Serve_Request(client);
    Free_Client(client);
    return 0;
}
