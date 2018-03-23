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

Server *newServer(uint16_t port,
        bool (*handler) (Server *, ReadCTX *, void *), void *aux) {
    Server *_this = (Server *)calloc(1, sizeof(Server));
    if(!_this) {
        fatal("error during creation of server");
    }

    _this->port = port;
    _this->handler = handler;

    signal(SIGPIPE, SIG_IGN);
    if ((_this->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fatal("socket() error");
    }

    int opts, optval = 1;
    if (setsockopt(_this->fd, SOL_SOCKET, SO_REUSEADDR,
                (const void *)&optval , sizeof(int)) < 0) {
        fatal("setsockopt() error");
    }

    if ((opts = fcntl(_this->fd, F_GETFL)) < 0
            || fcntl(_this->fd, F_SETFL, opts | O_NONBLOCK)) {
        fatal("fcntl() error");
    }
    _this->aux = aux;
    _this->server_addr.sin_family = AF_INET;
    _this->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    _this->server_addr.sin_port = htons(port);
    if (bind(_this->fd, (struct sockaddr *)&_this->server_addr,
                sizeof(_this->server_addr)) < 0) {
        fatal("bind() error");
    }

    if (listen(_this->fd, 100) < 0) {
        fatal("listen() error");
    }

    _this->maxfd = _this->fd;
    FD_ZERO(&_this->allfds);
    ServerFDadd(_this, _this->fd);

    return _this;
}

static void ReadAct(Server *_this) {
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

    readfds = _this->allfds;
    nready = select(_this->maxfd, &readfds, (fd_set *)0, (fd_set *)0, &tv);

    if (FD_ISSET(_this->fd, &readfds)) {
        client_fd = accept(_this->fd,
                (struct sockaddr *)&client_addr, (socklen_t *)&len);
        ServerFDadd(_this, client_fd);
        return;
    }

    ReadCTX *ctx = _this->rCTXHead;
    for (cn = _this->fd;
            cn < _this->maxfd && nready != 0;
            cn++, ctx = ctx->next) {
        if (FD_ISSET(cn, &readfds)) {
            if ((nbytes = read(cn, buf, MAXLINE-1)) > 0) {
                ctx->buffer = (char *)realloc(ctx->buffer, ctx->sz + nbytes);
                memcpy(ctx->buffer + ctx->sz, buf, nbytes);
                ctx->sz += nbytes;
            }
            if(nbytes == 0  ||
                    (nbytes == -1 && errno != EAGAIN) ||
                    !_this->handler(_this, ctx, _this->aux)) {
                ServerFDremove(_this, cn);
            }
            nready--;
        }
    }
}

static void WriteAct(Server *_this) {
    fd_set writefds;
    static struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200;

    writefds = _this->allfds;
    if (select(_this->maxfd, (fd_set *)0, &writefds, (fd_set *)0, &tv) > 0) {
        WriteCTX *ctx = _this->wCTXHead;
        while (ctx) {
            ctx = FD_ISSET(ctx->fd, &writefds) ?
                flushCTX(_this, ctx) : ctx->next;
        }
    }
}

void ServerLoop(Server *_this)
{
    while (true) {
        ReadAct(_this);
        WriteAct(_this);
    }
}

void ServerFDadd(Server *_this, int32_t fd)
{
    int opts;
    if ((opts = fcntl(fd, F_GETFL)) < 0 ||
            fcntl(fd, F_SETFL, opts | O_NONBLOCK)) {
        fatal("fcntl() fail");
    }

    ReadCTX *rCTX;
    FD_SET(fd, &_this->allfds);
    if (fd > _this->maxfd - 1) {
        _this->maxfd = fd + 1;
        rCTX = (ReadCTX *)calloc(1, sizeof(ReadCTX));
        rCTX->fd = fd;

        if (_this->rCTXTail == NULL) {
            _this->rCTXHead = _this->rCTXTail = rCTX;
        } else {
            _this->rCTXTail->next = rCTX;
            _this->rCTXTail = rCTX;
        }
    }
}

void ServerFDremove(Server *_this, int32_t fd)
{
    FD_CLR(fd, &_this->allfds);
    close(fd);

    int i;
    ReadCTX *rCTX = _this->rCTXHead;
    for (i = _this->fd + 1; i < fd; rCTX = rCTX->next, i++);
    if (rCTX->buffer) {
        free(rCTX->buffer);
        rCTX->buffer = NULL;
        rCTX->sz = 0;
        rCTX->pos = 0;
    }

    WriteCTX *ctx = _this->wCTXHead;
    WriteCTX *ptr;
    while (ctx) {
        ptr = ctx;
        ctx = ctx->next;
        if (ptr->fd == fd) {
            removeCTX(_this, ptr);
        }
    }
}
