#include "server.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int CTXRead(ReadCTX *ctx, char *buffer, size_t size) {
    if (CTXGetsz(ctx) < size) {
        return -1;
    }

    memcpy(buffer, ctx->buffer + ctx->pos, size);
    ctx->pos += size;
    return size;
}

int CTXGetsz(ReadCTX *ctx) {
    return ctx->sz - ctx->pos;
}

void CTXDiscard(ReadCTX *ctx) {
    int left = CTXGetsz(ctx);
    void *nptr = malloc(left);
    memcpy(nptr, ctx->buffer + ctx->pos, left);
    free(ctx->buffer);

    ctx->sz = left;
    ctx->pos = 0;
    ctx->buffer = nptr;
}

void ServerWrite(Server *server, int fd, char *buffer, size_t size) {
    WriteCTX *wCTX = calloc(1, sizeof(WriteCTX));
    wCTX->fd = fd;
    wCTX->buffer = malloc(size);
    if (wCTX->buffer) {
        memcpy(wCTX->buffer, buffer, size);
        wCTX->size = size;
        addCTX(server, wCTX);
    } else {
        free(wCTX);
    }
}

void addCTX(Server *server, WriteCTX *wCTX) {
    if (server->wCTXHead == NULL) {
        server->wCTXHead = server->wCTXTail = wCTX;
    } else {
        server->wCTXTail->next = wCTX;
        wCTX->prev = server->wCTXTail;
        server->wCTXTail = wCTX;
    }
}

WriteCTX *flushCTX(Server *server, WriteCTX *ctx) {
    WriteCTX *wctx = ctx;

    int nbytes = write(ctx->fd, ctx->buffer + ctx->pos, ctx->size - ctx->pos);
    if (nbytes == 0 ||
            (nbytes == -1 && errno != EAGAIN)) {
        while(wctx->next || wctx->fd != ctx->fd)
            wctx = wctx->next;
        ServerFDremove(server, ctx->fd);
    } else {
        wctx = wctx->next;
        if (nbytes >= ctx->size - ctx->pos) {
            removeCTX(server, ctx);
        } else {
            ctx->pos += nbytes;
        }
    }
    return wctx;
}


void removeCTX(Server *server, WriteCTX *ctx) {
    if (ctx == server->wCTXHead) {
        server->wCTXHead = ctx->next;
    }
    if (ctx == server->wCTXTail) {
        server->wCTXTail = ctx->prev;
    }
    if(ctx->buffer) {
        free(ctx->buffer);
    }
    free(ctx);
}
