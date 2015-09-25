/*
 * macro.h
 *
 *  Created on: Nov 5, 2014
 *      Author: daxu
 */

#ifndef MACRO_H_
#define MACRO_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>

#include <netdb.h>
#include <netinet/in.h>

#define DEBUG(M, ...) ;

/*
#define DEBUG(M, ...) fprintf(stderr, "%s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__);
*/
/**
 * Display message M, and system error, and exit program.
 */
#define ERROR(M) {fprintf(stderr, "%s:%d: " M " %d\n", __FILE__, __LINE__, errno);\
                  perror("\tmeaning");\
                  fprintf(stderr, "exiting program!\n");\
                  exit(EXIT_FAILURE);}
/**
 * Check the value of A. If A is less than zero, display message M,
 * and exit current program.
 */
#define CHECK(A, M) if((A)<0) {fprintf(stderr, M ", error %d\n", errno);\
                                perror("meaning");\
                                exit(EXIT_FAILURE);}

#define PORT_ID             10889   /* default port id */
#define BUFFER_SIZE         1024    /* the maximum size for file write and read */
#define DATE_LN             100     /* the maximum length for file date string */

/*The message passing between the client and server*/
struct fs_msg
{
    uint32_t client_id;     /* client id using pid_t?*/
    uint32_t fd;            /* file descriptor in client message,
                               return values of file system call in server message */
    uint8_t op;             /* operation code */
    uint16_t msg_len;       /* the length of the message */
    uint8_t msg[1];         /* message body, could be file name,
                               file content, or strings of file status */
}__attribute__((packed));
/*the maximum length of the message*/
#define MSG_SIZE            (sizeof(struct fs_msg) + BUFFER_SIZE)
/**
 * Get the length of a particular message.
 * @param m: message
 * @return size of the message
 */
static inline int sizeof_fs_msg(struct fs_msg *m)
{
    return sizeof(struct fs_msg) + m->msg_len;
}
/**
 * Check whether the received message is complete. If it is complete, convert
 * the integer data in the message to local byte order. Otherwise, the message
 * is not changed.
 * @param packet: the received message packet
 * @param recv_len: the length of the received message
 * @return -1 if the packet is not complete, 0 otherwise.
 */
static inline int decode(struct fs_msg *packet, size_t recv_len)
{
    if (recv_len < sizeof(struct fs_msg))
    {
        DEBUG("Incomplete message!")
        return -1;
    }
    packet->msg_len = ntohs(packet->msg_len);
    if (recv_len < sizeof(struct fs_msg) + packet->msg_len)
    {
        packet->msg_len = htons(packet->msg_len);
        DEBUG("Incomplete message!")
        return -1;
    }
    packet->client_id = ntohl(packet->client_id);
    packet->fd = ntohl(packet->fd);
    return 0;
}
/**
 * Convert the integer data in the message to network byte order.
 * @param packet: the message packet to be converted
 * @return 0
 */
static inline int encode(struct fs_msg *packet)
{
    struct fs_msg *data = (struct fs_msg *)packet;
    data->client_id = htonl(data->client_id);
    data->fd = htonl(data->fd);
    data->msg_len = htons(data->msg_len);
    return 0;
}

/* input   : hostname in ip format such as cs.rutgers.edu
 * returns : 0 if any error ipaddr otherwise
 * output  : -
 * desc    : resolves an ip name.
 */
static int host2IpAddr(char *anIpName)
{
    struct hostent *hostEntry;
    struct in_addr *scratch;

    if ((hostEntry = gethostbyname(anIpName)) == (struct hostent*) NULL)
        return 0;
    scratch = (struct in_addr *) hostEntry->h_addr;
    return (ntohl(scratch->s_addr));
}
#endif /* MACRO_H_ */
