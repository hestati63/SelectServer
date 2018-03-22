#ifndef __CTX__
#define __CTX__
#define MAXLINE 4096
#include <stdint.h>

typedef struct _RCTX
{
     char* buffer;
     int32_t fd;
    uint32_t sz;
    uint32_t pos;

    struct _RCTX *next;
} ReadCTX;

typedef struct _WCTX
{
    struct _WCTX *next;
    struct _WCTX *prev;
    char* buffer;
    int32_t fd;
    int32_t size;
    int32_t pos;
} WriteCTX;

#include "server.h"
struct _server;
typedef struct _server Server;

int CTXRead(ReadCTX *, char *, size_t);      /* read from CTX */
int CTXGetsz(ReadCTX *);                     /* get left character from CTX */
void CTXDiscard(ReadCTX *);                   /* discard unused buf */
void ServerWrite(Server *, int, char *, size_t); /* write data to CTX*/

void addCTX(Server *, WriteCTX *);              /* manage writeCTX */
void removeCTX(Server *, WriteCTX *);
WriteCTX *flushCTX(Server *, WriteCTX *);       /* free WriteCTX and return ctx->next */
#endif


