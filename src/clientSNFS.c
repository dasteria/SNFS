/*
 * clientSNFS.c
 *
 *  Created on: Nov 5, 2014
 *      Author: daxu
 */

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#include "macro.h"

/**
 * Check the value of A. If A is less than zero, display message M,
 * close current session socket and return -1..
 */
#define CHECK_ERR(A, M) if((A)<0) {fprintf(stderr, M ", error %d\n", errno);\
                                perror("meaning");\
                                DEBUG("Closing connection to server")\
                                close(session_socket);\
                                return -1;}

struct fileStat
{
    off_t size;                  /* total size, in bytes */
    char birthtime[DATE_LN];     /* time of creation */
    char atime[DATE_LN];         /* time of last access */
    char mtime[DATE_LN];         /* time of last modification */
};

static int client_id = -1;          /* a unique client id */
static struct sockaddr_in server_addr;

/**
 * Send client request to server, and wait for the request from server
 * @param message: the message sent to server, and received message
 * @return -1 when communication failed, otherwise the file descriptor
 */
static int handle_request(struct fs_msg *message)
{
    int session_socket, msg_size, cur_size, total_size, complete;
    CHECK_ERR(session_socket = socket(AF_INET, SOCK_STREAM, 0), "opening client socket")
    msg_size = sizeof_fs_msg(message);
    encode(message);
    CHECK_ERR(connect(session_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)), "connecting to sercer")
    DEBUG("Sending request to server")
    for(total_size = 0; total_size < msg_size; total_size += cur_size)
    {
        cur_size = write(session_socket, (uint8_t *)message + total_size, msg_size-total_size);
        CHECK_ERR(cur_size, "sending data to server")
    }
    memset((void*) message, 0, MSG_SIZE);
    DEBUG("Waiting for response from server")
    total_size = 0;
    do{
        cur_size = read(session_socket, (uint8_t *) message + total_size,
                MSG_SIZE);
        CHECK_ERR(cur_size, "reading data from client")
        total_size += cur_size;
        complete = decode(message, total_size);
    } while(complete == -1);
    /*
    for(total_size = 0, cur_size = -1; cur_size; total_size += cur_size)
    {
        cur_size = read(session_socket, (uint8_t *)message + total_size, MSG_SIZE);
        CHECK_ERR(cur_size, "reading data from server")
    }*/
    close(session_socket);
    DEBUG("Connection closed")
    if (complete < 0)
    {
        DEBUG("Corrupted data was received")
        return -1;
    }else
    {
        DEBUG("Data successfully received")
        return message->fd;
    }
}

/*
 * This method is called to provide the server IP and port addresses.
 * The character string passed to this routine may contain a valid IP address
 * (e.g., 192.168.1.1) or a hostname (e.g., scarlet.rutgers.edu).
 * Your code must handle both cases.
 * You can assume that the setServer() call is made before the openFile().
 */
void setServer(char *serverIP, int port)
{
    int temp;

    memset((void*) &server_addr, 0, sizeof(server_addr));
    temp = inet_pton(AF_INET, serverIP, &server_addr.sin_addr);
    if (temp < 0)
    {
        fprintf(stderr, "resolving host name, error %d\n", errno);
        perror("meaning");
        return;
    }
    if (temp == 0)
    {
        struct hostent *hostEntry;
        if (!(hostEntry = gethostbyname(serverIP)))
        {
            printf("not a valid network address");
            return;
        }
        server_addr.sin_addr.s_addr = ((struct in_addr *)hostEntry->h_addr)->s_addr;
    }
    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;
    srand(time(NULL));
    client_id = (getpid() & 0xFFF) | ((rand()%0x8FFFF) << 12);
    DEBUG("client id:%d", client_id)
    return;
}

/* takes the name of a file and returns a file descriptor or -1 on failure.
 * If the file doesn't exist, it creates the file.
 */
int openFile(char *name)
{
    if (client_id == -1) return -1;
    uint8_t buffer[MSG_SIZE] = {0};
    struct fs_msg *message = (struct fs_msg *)buffer;
    message->client_id = client_id;
    message->fd = -1;
    message->op = 'o';
    message->msg_len = strlen(name);
    if (message->msg_len > BUFFER_SIZE)
    {
        DEBUG("File name too long")
        return -1;
    }
    strcpy((char *)message->msg, name);
    return handle_request(message);
}

/* attempts to read the whole file from file descriptor fd into the buffer
 * starting at buf. On success, the number of bytes read is returned and -1
 * otherwise. You can assume file size will be less than 1KB.
 */
int readFile(int fd, void *buf)
{
    if (client_id == -1) return -1;
    uint8_t buffer[MSG_SIZE] = {0};
    struct fs_msg *message = (struct fs_msg *)buffer;
    message->client_id = client_id;
    message->fd = fd;
    message->op = 'r';
    message->msg_len = 0;
    if (handle_request(message)!= -1)
    {
        strcpy(buf, (char *)message->msg);
        return message->fd;
    }
    else return -1;
}

/* writes the whole file from the buffer (pointed to by buf) to the file
 * referred by the file descriptor fd. On success, the number of bytes written
 * is returned and -1 otherwise.
 */
int writeFile(int fd, void *buf)
{
    if (client_id == -1) return -1;
    uint8_t buffer[MSG_SIZE] = {0};
    struct fs_msg *message = (struct fs_msg *)buffer;
    message->client_id = client_id;
    message->fd = fd;
    message->op = 'w';
    message->msg_len = strlen(buf);
    if (message->msg_len > BUFFER_SIZE)
    {
        DEBUG("Assuming writing string, and data too long")
        return -1;
    }
    strcpy((char *)message->msg, buf);
    return handle_request(message);
}

/* returns information about a file. You need to define structure fileStat.
 * It should at least contain file size, creation time, access time and
 * modification time.
 */
int statFile(int fd, struct fileStat *buf)
{
    char *date_offset;
    if (client_id == -1) return -1;
    uint8_t buffer[MSG_SIZE] = {0};
    struct fs_msg *message = (struct fs_msg *)buffer;
    message->client_id = client_id;
    message->fd = fd;
    message->op = 's';
    message->msg_len = 0;
    if (handle_request(message) == -1) return -1;
    else
    {
        sscanf((char *)message->msg, "%lld",(long long*)&(buf->size));
        date_offset = (char*)message->msg + strlen((char*)message->msg) + 1;
        strcpy(buf->birthtime, date_offset);
        date_offset += strlen(date_offset) + 1;
        strcpy(buf->atime, date_offset);
        date_offset += strlen(date_offset) + 1;
        strcpy(buf->mtime, date_offset);
        return 0;
    }
}

/* relinquishes all control that the client had with the file.
 * For example, after closing the file if client calls readFile,
 * the request should fail.
 */
int closeFile(int fd)
{
    if (client_id == -1) return -1;
    uint8_t buffer[MSG_SIZE] = {0};
    struct fs_msg *message = (struct fs_msg *)buffer;
    message->client_id = client_id;
    message->fd = fd;
    message->op = 'c';
    message->msg_len = 0;
    return handle_request(message);
}
