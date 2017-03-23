#ifndef __SSS__
#define __SSS__

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

typedef struct
{
     int8_t* buffer;
     int32_t fd;
    uint32_t sz;
    uint32_t pos;
} SelectCTX;

typedef struct
{
    uint16_t fd;
    uint16_t port;
    struct sockaddr_in server_addr;

    bool (*handler) (SelectCTX *);                  /* handler for packet  should return True on success*/

    /* For select */
    int32_t maxfd;
    fd_set readfds;
    SelectCTX **CTXList;                         /* scalable Context List */
} Server;


Server *newServer(uint16_t port, bool (*handler) (SelectCTX *));

void ServerLoop(Server *);

void ServerFDadd(Server *, int32_t);
#endif
