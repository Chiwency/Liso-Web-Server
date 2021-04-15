#include "./csapp.h"
#include <sys/select.h>
#include "../src/parse.h"

#define STATUS_OK "200"
#define STATUS_BAD_REQUEST "400"
#define STATUS_NOT_FOUND "404"
#define STATUS_REQUEST_TIME_OUT "408"
#define STATUS_SERVER_ERROR "500"
#define STATUS_NOT_IMPLIMENT "501"
#define STATUS_VERSION_NOT_SUPPPORTED "505"

#define MAXSIZE 8192

typedef struct
{
    int fd;
    char *body;
    char buf[MAXSIZE];
    Request *request;
} Client;

// 连接池
typedef struct
{
    fd_set read_fds;
    Client *client_fd[FD_SETSIZE];
} ConnectionPool;

int Parse_Header(Client *client);
void Free_Client(Client *client);
void Send_Ressponse(int fd, char *errnum,
                    char *shortmsg, char *body);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
void serverLog(char *type, char *info);
void read_requesthdrs(rio_t *rp, char *buff);
void get_filetype(char *filename, char *filetype);
void Serve_Request(Client *client);

ConnectionPool *Init_Connection_Pool(int fd);