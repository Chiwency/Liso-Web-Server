#include "../include/handler.h"

int handleClient(Client *client);



int main()
{
    daemonize();
    struct timeval tv;
    int listenfd, connfd;
    SSL *client_context;
    SSL_CTX *ssl_context;

    fd_set read_fd_set;
    ConnectionPool *connPool;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    listenfd = Open_listenfd(PORT);
    connPool = Init_Connection_Pool(listenfd);
    int closed = 0, opend = 0;
    while (1)
    {
        // select()会原地修改fd_set的值，所以每次调用要重新初始化
        read_fd_set = connPool->read_fds;
        int n;
        if ((n = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &tv)) < 0)
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
                    if (Init_Client(connPool) > 0)
                    {
                        opend++;
                        printf("open:%d\n", opend);
                    }
                }
                else if (handleClient(connPool->client_fd[i]) == 0)
                {
                    Free_Client(connPool, i);
                    closed++;
                    printf("close:%d\n", closed);
                }
            }
            else if (n == 0 && connPool->client_fd[i]) // timeout时间内未收到数据，关闭连接池内的连接.
            {
                Free_Client(connPool, i);
                closed++;
                printf("close:%d\n", closed);
            }
        }
    }
    liso_shutdown();
    return 0;
}

int handleClient(Client *client)
{
    char buf[MAXSIZE];
    memset(buf, 0, MAXSIZE);
    SSL_read(client->client_context, buf, MAXSIZE);
    strcat(client->buf, buf);
    while (1)
    {
        if (client->request)
        {
            int code = Handle_Request(client);
            if (code == 1)
            {
                return 1;
            }
        }
        char *p = NULL;
        if (!(p = strstr(client->buf, "\r\n\r\n")))
        {
            // 没有读完请求首部，回去继续等待后续数据
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
            clienterror(client->client_context, NULL, STATUS_BAD_REQUEST, "Bad Request",
                        "Bad Request, Please try again");
            return 0;
        }
        p++;
        strcpy(buf, p);
        strcpy(client->buf, buf); // 提取请求首部后，把后续数据再放入client->buf中
    }

    return 0;
}
