#include "server.h"
#include "util.h"
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdio.h>
#include <fcntl.h>

Server *newServer(uint16_t port, bool (*handler) (SelectCTX *))
{
    Server *this = (Server *)calloc(1, sizeof(Server));
    if(!this)
        fatal("error during creation of server");

    uint32_t server_fd;
    this->port = port;
    this->handler = handler;

    signal(SIGPIPE, SIG_IGN);
    if((this->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        fatal("socket() error");

    int opts, optval = 1;
    if(setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
        fatal("setsockopt() error");

    if((opts = fcntl(this->fd, F_GETFL)) < 0 || fcntl(this->fd, F_SETFL, opts | O_NONBLOCK))
        fatal("fcntl() error");

    this->server_addr.sin_family = AF_INET;
    this->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    this->server_addr.sin_port = htons(port);
    if(bind(this->fd, (struct sockaddr *)&this->server_addr, sizeof(this->server_addr)) < 0)
        fatal("bind() error");

    if(listen(this->fd, 100) < 0)
        fatal("listen() error");

    this->maxfd = this->fd;
    FD_ZERO(&this->readfds);
    ServerFDadd(this, this->fd);

    return this;
}

void ServerLoop(Server *this)
{
    fd_set allfds;
    int nready;
    static struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200;

    struct sockaddr_in client_addr;
    int len = sizeof(client_addr);
    int client_fd;
    int opts;

    #define MAXLINE 4096
    int nbytes, cn;
    char buf[MAXLINE];
    SelectCTX *ctx;

    while(1)
    {
        allfds = this->readfds;
        nready = select(this->maxfd, &allfds, (fd_set *)0, (fd_set *)0, &tv);

        if(FD_ISSET(this->fd, &allfds))
        {
            client_fd = accept(this->fd, (struct sockaddr *)&client_addr, (socklen_t *)&len);
            if((opts = fcntl(client_fd, F_GETFL)) < 0 || fcntl(client_fd, F_SETFL, opts | O_NONBLOCK))
                fatal("fcntl() fail");
            ServerFDadd(this, client_fd);
            continue;
        }

        for(cn = 0; cn < this->maxfd && nready != 0; cn++)
        {
            if(FD_ISSET(cn, &allfds))
            {
                ctx = this->CTXList[cn];
                while((nbytes = read(cn, buf, MAXLINE)) > 0)
                {
                    ctx->buffer = realloc(ctx->buffer, ctx->sz + nbytes);
                    memcpy(ctx->buffer + ctx->sz, buf, nbytes);
                    ctx->sz += nbytes;
                }

                if(nbytes == 0  ||
                        (nbytes == -1 && errno != EAGAIN) ||
                        !this->handler(this->CTXList[cn]))
                {
                    if(this->CTXList[cn]->buffer)
                        free(this->CTXList[cn]->buffer);
                    FD_CLR(cn, &this->readfds);
                    close(cn);
                }
                nready--;
            }
        }
    }
}

void ServerFDadd(Server *this, int32_t fd)
{
    FD_SET(fd, &this->readfds);
    if(fd + 1 > this->maxfd)
    {
        this->maxfd = fd + 1;
        this->CTXList = realloc(this->CTXList, sizeof(SelectCTX *) * this->maxfd);
    }

    if(this->CTXList[fd] == NULL)
        this->CTXList[fd] = malloc(sizeof(SelectCTX));

    bzero(this->CTXList[fd], sizeof(SelectCTX));
    this->CTXList[fd]->fd = fd;
}

