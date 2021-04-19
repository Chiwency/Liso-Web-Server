#include "./csapp.h"
#include <sys/select.h>
#include "../src/parse.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <syslog.h>

#define STATUS_OK "200"
#define STATUS_BAD_REQUEST "400"
#define STATUS_NOT_FOUND "404"
#define STATUS_REQUEST_TIME_OUT "408"
#define STATUS_SERVER_ERROR "500"
#define STATUS_NOT_IMPLIMENT "501"
#define STATUS_VERSION_NOT_SUPPPORTED "505"

#define LOCKFILE "/Users/mac/c/liso-web-server/p1/liso.lock"
#define LOGFILE  "/Users/mac/c/liso-web-server/p1/logfile.log"
#define PORT "9999"
#define PRIVATE_KEY_PATH "/Users/mac/c/liso-web-server/p1/localhost.key"
#define PUBLIC_KEY_PATH "/Users/mac/c/liso-web-server/p1/localhost.crt"

#define MAXSIZE 88000

typedef struct
{
    int fd;
    char *body;
    char buf[MAXSIZE];
    SSL *client_context;
    Request *request;
} Client;

// 连接池
typedef struct
{
    int lintenfd;
    SSL_CTX *ssl_context;
    fd_set read_fds;
    Client *client_fd[FD_SETSIZE];
} ConnectionPool;

int Handle_Request(Client *client);

void Send_Ressponse(SSL *client_context, char *errnum,
                    char *shortmsg, char *body);
void clienterror(SSL *client_context, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
void serverLog(char *type, char *info);
void read_requesthdrs(rio_t *rp, char *buff);
void get_filetype(char *filename, char *filetype);
void Serve_Request(Client *client);

ConnectionPool *Init_Connection_Pool(int fd);
SSL_CTX *Init_SSL();
int Init_Client(ConnectionPool *connPool);
void signal_handler(int sig);
int daemonize();
void liso_shutdown();
void Free_Request(Client *client);

void Free_Client(ConnectionPool *connPool, int i);