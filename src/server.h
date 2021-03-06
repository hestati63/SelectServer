#ifndef __SSS__
#define __SSS__
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "context.h"
#include <future>

typedef struct _server
{
    int32_t fd;
    uint16_t port;
    struct sockaddr_in server_addr;

    /*
     * Paket Listener handler
     * if function return
     * -> True then keep connection
     * -> False then close connection
     */
    bool (*handler) (struct _server *, ReadCTX *, void *aux);

    /* For select */
    int32_t maxfd;
    fd_set allfds;

    /* readCTXList */
    ReadCTX *rCTXHead;
    ReadCTX *rCTXTail;

    /* writeCTXList */
    WriteCTX *wCTXHead;
    WriteCTX *wCTXTail;

    /* AUX*/
    void *aux;
} Server;


Server *newServer(uint16_t port,
        bool (*handler) (Server *, ReadCTX *, void *), void *aux);

void ServerLoop(Server *);
void ServerFDadd(Server *, int32_t);
void ServerFDremove(Server *, int32_t);
#endif
