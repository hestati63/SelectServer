#ifndef __SSS__
#define __SSS__
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "context.h"
typedef struct _server
{
    uint16_t fd;
    uint16_t port;
    struct sockaddr_in server_addr;

    bool (*handler) (struct _server *, int);                  /* handler for packet should return True on success*/

    /* For select */
    int32_t maxfd;
    fd_set allfds;
    ReadCTX *rCTXHead;      /* readCTXList */
    ReadCTX *rCTXTail;      /* readCTXList */

    WriteCTX *wCTXHead;
    WriteCTX *wCTXTail;                            /* writeCTX linked_list */
} Server;


Server *newServer(uint16_t port, bool (*handler) (Server *, int));

void ServerLoop(Server *);

void ServerFDadd(Server *, int32_t);
void ServerFDremove(Server *, int32_t);
#endif
