#include "protocol.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <debug.h>

int proto_send_packet(int fd, CHLA_PACKET_HEADER *hdr, void *payload)
{
    debug("PROTO SEND PACKET");
    ssize_t byteWritten = -1;
    if ((byteWritten = write(fd, hdr, sizeof(CHLA_PACKET_HEADER))) < 0)
    {
        perror("WRITE HEADER ERROR");
        errno = EIO;
        return -1;
    }
    if (byteWritten != sizeof(CHLA_PACKET_HEADER))
    {
        perror("WRITE HEADER SHORTCOUNT");
        errno = EIO;
        return -1;
    }

    if (ntohl(hdr->payload_length) > 0)
    {
        if ((byteWritten = write(fd, payload, ntohl(hdr->payload_length))) < 0)
        {
            perror("WRITE PAYLOAD ERROR");
            errno = EIO;
            return -1;
        }
        if (byteWritten != ntohl(hdr->payload_length))
        {
            perror("WRITE PAYLOAD SHORTCOUNT");
            errno = EIO;
            return -1;
        }
    }

    return 0;
}

int proto_recv_packet(int fd, CHLA_PACKET_HEADER *hdr, void **payload)
{
    debug("PROTO RECV PACKET");
    ssize_t byteRead = -1;
    if ((byteRead = read(fd, hdr, sizeof(CHLA_PACKET_HEADER))) < 0)
    {
        debug("READ HEADER ERROR");
        errno = EIO;
        return -1;
    }
    if (byteRead != sizeof(CHLA_PACKET_HEADER))
    {
        debug("BYTES READ %lu\n", byteRead);
        debug("READ HEADER SHORTCOUNT");
        errno = EIO;
        return -1;
    }
    debug("PROTO READ HEADER");
    if (ntohl(hdr->payload_length) > 0)
    {
        *payload = malloc(ntohl(hdr->payload_length));
        if ((byteRead = read(fd, *payload, ntohl(hdr->payload_length))) < 0)
        {
            debug("READ PAYLOAD ERROR");
            free(*payload);
            errno = EIO;
            return -1;
        }
        if (byteRead != ntohl(hdr->payload_length))
        {
            debug("READ PAYLOAD SHORTCOUNT");
            free(*payload);
            errno = EIO;
            return -1;
        }
    }
    else
    {
        *payload = NULL;
    }
    /*
    debug("TYPE%u", hdr->type);
    debug("LENGTH%u", ntohl(hdr->payload_length));
    debug("MSGID%d", ntohl(hdr->msgid));
    debug("FD%d", fd);
    debug("DATA%p", *payload);
    */
    return 0;
}