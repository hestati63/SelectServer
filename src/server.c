#include "server.h"
#include "context.h"
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

Server *newServer(uint16_t port, bool (*handler) (Server *, ReadCTX *))
{
    Server *this = (Server *)calloc(1, sizeof(Server));
    if(!this)
        fatal("error during creation of server");

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
    FD_ZERO(&this->allfds);
    ServerFDadd(this, this->fd);

    return this;
}

static void ReadAct(Server *this)
{
    fd_set readfds;
    int nready;
    static struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200;

    struct sockaddr_in client_addr;
    int len = sizeof(client_addr);
    int client_fd;

    int nbytes, cn;
    char buf[MAXLINE];

    readfds = this->allfds;
    nready = select(this->maxfd, &readfds, (fd_set *)0, (fd_set *)0, &tv);

    if(FD_ISSET(this->fd, &readfds))
    {
        client_fd = accept(this->fd, (struct sockaddr *)&client_addr, (socklen_t *)&len);
        ServerFDadd(this, client_fd);
        return;
    }

    ReadCTX *ctx = this->rCTXHead;
    for(cn = this->fd; cn < this->maxfd && nready != 0; cn++, ctx = ctx->next)
    {
        if(FD_ISSET(cn, &readfds))
        {
            if((nbytes = read(cn, buf, MAXLINE-1)) > 0)
            {
                ctx->buffer = realloc(ctx->buffer, ctx->sz + nbytes);
                memcpy(ctx->buffer + ctx->sz, buf, nbytes);
                ctx->sz += nbytes;
            }
            if(nbytes == 0  ||
                    (nbytes == -1 && errno != EAGAIN) ||
                    !this->handler(this, ctx))
                ServerFDremove(this, cn);
            nready--;
        }
    }
}

static void WriteAct(Server *this)
{
    fd_set writefds;
    static struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200;

    writefds = this->allfds;
    if(select(this->maxfd, (fd_set *)0, &writefds, (fd_set *)0, &tv) > 0)
    {
        WriteCTX *ctx = this->wCTXHead;
        while(ctx)
            ctx = FD_ISSET(ctx->fd, &writefds) ?
                flushCTX(this, ctx) : ctx->next;
    }
}

void ServerLoop(Server *this)
{
    for(;;)
    {
        ReadAct(this);
        WriteAct(this);
    }
}

void ServerFDadd(Server *this, int32_t fd)
{
    int opts;
    if((opts = fcntl(fd, F_GETFL)) < 0 ||
            fcntl(fd, F_SETFL, opts | O_NONBLOCK))
        fatal("fcntl() fail");

    ReadCTX *rCTX;
    FD_SET(fd, &this->allfds);
    if(fd > this->maxfd - 1)
    {
        this->maxfd = fd + 1;
        rCTX = calloc(1, sizeof(ReadCTX));
        rCTX->fd = fd;

        if(this->rCTXTail == NULL)
        {
            this->rCTXHead = this->rCTXTail = rCTX;
        }
        else
        {
            this->rCTXTail->next = rCTX;
            this->rCTXTail = rCTX;
        }
    }
}

void ServerFDremove(Server *this, int32_t fd)
{
    FD_CLR(fd, &this->allfds);
    close(fd);

    int i;
    ReadCTX *rCTX = this->rCTXHead;
    for(i = this->fd + 1; i < fd; rCTX = rCTX->next, i++);
    if(rCTX->buffer)
    {
        free(rCTX->buffer);
        rCTX->buffer = NULL;
        rCTX->sz = 0;
        rCTX->pos = 0;
    }

    WriteCTX *ctx = this->wCTXHead;
    WriteCTX *ptr;
    while(ctx)
    {
        ptr = ctx;
        ctx = ctx->next;
        if(ptr->fd == fd)
            removeCTX(this, ptr);
    }
}

