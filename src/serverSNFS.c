/*
 * serverSNFS.c
 *
 *  Created on: Nov 5, 2014
 *      Author: daxu
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/stat.h>

#include "macro.h"

/**
 * Check the value of A. If A is less than zero, close server socket and
 * exit program.
 */
#define CHECK_N(A, M) if((A)<0) {fprintf(stderr, M ", error %d\n", errno);\
                                perror("meaning");\
                                DEBUG("Closing server socket")\
                                close(server_socket);\
                                DEBUG("Program exiting")\
                                exit(EXIT_FAILURE);}
/**
 * Check the value of A. If A is less than zero, display message M,
 * close current session socket and exit current thread.
 */
#define CHECK_ERR(A, M) if((A)<0) {fprintf(stderr, M ", error %d\n", errno);\
                                perror("meaning");\
                                DEBUG("Closing session socket")\
                                close(session_socket);\
                                DEBUG("Thread exiting")\
                                pthread_exit(NULL);}
/**
 * Check the value of A. If A is less than zero, display message M,
 * set file descriptor to -1, exit current loop..
 */
#define CHECK_F(A, M) if((A)<0) {fprintf(stderr, M ", error %d\n", errno);\
                                perror("meaning");\
                                message->fd = -1;\
                                break;}

#define FILE_SYS_PATH_LN    100
#define QUEUE_LN            30
#define HASH_BITS           8
#define HASH_SZ             (1 << HASH_BITS)
#define HASH_MASK           (HASH_SZ - 1)
#define _hashfn(fd)         (((fd >> HASH_BITS) ^ fd) & HASH_MASK)
#define HASH_ENTRY(fd)      (hash_table + _hashfn(fd))

struct client_struct
{
    int client_id;
};

struct fd_struct
{
    struct client_struct client;
    /* Hash table maintenance information */
    struct fd_struct *next;
    int fd;
};

pthread_mutex_t file_l, hash_table_l;
static struct fd_struct *hash_table[HASH_SZ] = {NULL};

/**
 * Compare two clients a and p.
 * @param a: pointer to client a
 * @param b: pointer to client b
 * @return 1 when they are the same client, 0 otherwise.
 */
static inline int is_equal(struct client_struct *a, struct client_struct *b)
{
    return a->client_id == b->client_id;
}
/**
 * Copy client pointed by from to client pointed by to.
 * @param to: the target client
 * @param from: the source client
 */
static inline void client_cpy(struct client_struct *to, struct client_struct *from)
{
    if (to && from)
    {
        to->client_id = from->client_id;
    }
    return;
}

/*
 * Hash related routines must be called with the hash lock held!
 */
/**
 * Find a matching fd and associated client in the hash table
 * @param fd: the file descriptor
 * @param client: the client
 * @return 1 when a matching is found, 0 otherwise.
 */
static inline int hash_find(int fd, struct client_struct *client)
{
    struct fd_struct *hash_entry = *HASH_ENTRY(fd);
    for (; hash_entry; hash_entry = hash_entry->next)
        if (fd == hash_entry->fd) break;
    if (!hash_entry) return 0;
    return is_equal(client, &(hash_entry->client));
}
/**
 * Insert a fd and associated client into the hash table
 * @param fd: the file descriptor
 * @param client: the client
 */
static inline void hash_insert(int fd, struct client_struct *client)
{
    struct fd_struct *hash_entry = *HASH_ENTRY(fd);
    struct fd_struct *new_entry = malloc(sizeof(struct fd_struct));
    new_entry->fd = fd;
    client_cpy(&(new_entry->client), client);
    new_entry->next = hash_entry;
    *HASH_ENTRY(fd) = new_entry;
    return;
}
/**
 * Delete a fd and associated client from the hash table
 * @param fd: the file descriptor
 * @param client: the client
 */
static inline void hash_delete(int fd, struct client_struct *client)
{
    struct fd_struct *hash_entry = *HASH_ENTRY(fd);
    struct fd_struct *next_fd = hash_entry, *prev_fd = next_fd;
    for (; next_fd; next_fd = next_fd->next)
    {
        if (fd == next_fd->fd) break;
        prev_fd = next_fd;
    }
    if (prev_fd == next_fd) *HASH_ENTRY(fd) = next_fd->next;
    prev_fd->next = next_fd->next;
    free(next_fd);
    return;
}
/**
 * A wrapper of strftime. Use localtime to convert a time_t data to string data.
 * @param buffer: pointer to where the string will be stored.
 * @param rawtime: the source time_t structure.
 * @return the number of byte data written in the buffer
 */
static inline size_t time_to_str(char *buffer, time_t rawtime)
{
    return strftime(buffer, DATE_LN, "%c", localtime(&rawtime));
}

void open_file(struct client_struct *client, struct fs_msg *message)
{
    int fd;
    pthread_mutex_lock(&file_l);
    fd = open((char *)message->msg, O_CREAT | O_RDWR, 0755);
    pthread_mutex_unlock(&file_l);
    if (fd != -1)
    {
        pthread_mutex_lock(&hash_table_l);
        if (!hash_find(fd, client)) hash_insert(fd, client);
        pthread_mutex_unlock(&hash_table_l);
    }
    message->msg_len = 0;
    message->fd = fd;
    return;
}

void close_file(struct client_struct *client, struct fs_msg *message)
{
    int found = 0;
    message->msg_len = 0;
    pthread_mutex_lock(&hash_table_l);
    if (hash_find(message->fd, client))
    {
        found = 1;
        hash_delete(message->fd, client);
    }
    pthread_mutex_unlock(&hash_table_l);
    if (found)
    {
        pthread_mutex_lock(&file_l);
        message->fd = close(message->fd);
        pthread_mutex_unlock(&file_l);
    }
    return;
}

void read_file(struct client_struct *client, struct fs_msg *message)
{
    int found, fd, cur_size, total_size;
    fd = message->fd;
    message->msg_len = 0;
    pthread_mutex_lock(&hash_table_l);
    found = hash_find(fd, client);
    pthread_mutex_unlock(&hash_table_l);
    if (found)
    {
        pthread_mutex_lock(&file_l);
        for (total_size = 0, cur_size = -1; cur_size; total_size += cur_size)
        {
            cur_size = read(fd, (char *) message->msg + total_size,
                    BUFFER_SIZE - total_size);
            CHECK_F(cur_size, "reading file")
        }
        if(lseek(fd, 0, SEEK_SET)<0)
        {
            fprintf(stderr, "moving file offset to the beginning, error %d\n", errno);
            perror("meaning");
        }
        pthread_mutex_unlock(&file_l);
        if (message->fd != -1)
        {
            message->fd = message->msg_len = total_size;
        }
    }else
    {
        message->fd = -1;
    }
    return;
}

void write_file(struct client_struct *client, struct fs_msg *message)
{
    int found, fd, cur_size, total_size, msg_size;
    fd = message->fd;
    msg_size = message->msg_len;
    pthread_mutex_lock(&hash_table_l);
    found = hash_find(fd, client);
    pthread_mutex_unlock(&hash_table_l);
    if (found)
    {
        pthread_mutex_lock(&file_l);
        if(ftruncate(fd, 0)<0)
        {
            fprintf(stderr, "truncating file, error %d\n", errno);
            perror("meaning");
        }
        for(total_size = 0; total_size < msg_size; total_size += cur_size)
        {
            cur_size = write(fd, (char *)message->msg + total_size, msg_size-total_size);
            CHECK_F(cur_size, "writing file")
        }
        if(lseek(fd, 0, SEEK_SET)<0)
        {
            fprintf(stderr, "moving file offset to the beginning, error %d\n", errno);
            perror("meaning");
        }
        pthread_mutex_unlock(&file_l);
        if (message->fd != -1) message->fd = total_size;
    }else
    {
        message->fd = -1;
    }
    message->msg_len = 0;
    return;
}

void stat_file(struct client_struct *client, struct fs_msg *message)
{
    int found, fd, total_size;
    struct stat st;
    fd = message->fd;
    message->msg_len = 0;
    pthread_mutex_lock(&hash_table_l);
    found = hash_find(fd, client);
    pthread_mutex_unlock(&hash_table_l);
    if (found)
    {
        pthread_mutex_lock(&file_l);
        do
        {
            CHECK_F(fstat(fd, &st), "getting file status")
        }while(0);
        pthread_mutex_unlock(&file_l);
        if (message->fd != -1)
        {
            message->fd = 0;
            total_size = sprintf((char *)message->msg, "%lld",(long long)st.st_size) + 1;
            total_size += time_to_str((char *)message->msg + total_size, st.st_birthtime) +1;
            total_size += time_to_str((char *)message->msg + total_size, st.st_atime) + 1;
            total_size += time_to_str((char *)message->msg + total_size, st.st_mtime) + 1;
            message->msg_len = total_size;
        }
    }else
    {
        message->fd = -1;
    }
    return;
}

void arg_error(void)
{
    fprintf(stderr, "Usage serverSNFS <­-port port#> <-­mount filepath> \n");
    exit(EXIT_FAILURE);
}
/**
 * The child thread responsible for handing a client request.
 * @param client_socket: a opened valid client socket
 */
void handle_client(int *client_socket)
{
    int session_socket, msg_size, cur_size, total_size, complete;
    uint8_t buffer[MSG_SIZE] = {0};
    struct fs_msg *message = (struct fs_msg *)buffer;
    session_socket = (int) client_socket;
    DEBUG("waiting for response from client:%d", session_socket)
    total_size = 0;
    do{
        DEBUG("start reading:%d", session_socket)
        cur_size = read(session_socket, (uint8_t *) message + total_size,
                MSG_SIZE);
        CHECK_ERR(cur_size, "reading data from client")
        total_size += cur_size;
        complete = decode(message, total_size);
    } while(complete == -1);
    CHECK_ERR(complete, "received incomplete message")
    struct client_struct client;
    client.client_id = message->client_id;
    switch (message->op)
    {
    case 'o':
        DEBUG("opening file")
        open_file(&client, message);
        break;
    case 'c':
        DEBUG("closing file")
        close_file(&client, message);
        break;
    case 'r':
        DEBUG("reading file")
        read_file(&client, message);
        break;
    case 'w':
        DEBUG("writing file:%s", message->msg)
        write_file(&client, message);
        break;
    case 's':
        DEBUG("sending status of file")
        stat_file(&client, message);
        break;
    default:
        DEBUG("undefined access!")
    }
    msg_size = sizeof_fs_msg(message);
    encode(message);
    DEBUG("sending request to client")
    for(total_size = 0; total_size < msg_size; total_size += cur_size)
    {
        cur_size = write(session_socket, (uint8_t *)message + total_size, msg_size-total_size);
        CHECK_ERR(cur_size, "sending request to client")
    }
    DEBUG("terminating current connection")
    close(session_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in client_addr, server_addr;
    int server_socket, client_socket, client_addr_len, arg_count, server_port;
    char hostname[32], file_path[FILE_SYS_PATH_LN], *str_ptr;
    pthread_t session_thread;
    /*Prepare the listening port and a working directory*/
    server_port = PORT_ID;
    strcpy(file_path, "/tmp/server_daxu");
    /*if (!(argc == 1 || argc == 3 || argc == 5)) arg_error();*/
    if (argc != 5) arg_error();
    for (arg_count=1; arg_count<argc-1; arg_count+=2)
    {
        if (strcmp(argv[arg_count], "-port") == 0)
        {
            if (strtol(argv[arg_count+1], &str_ptr, 10) <= 0
                    || strtol(argv[arg_count+1], &str_ptr, 10) >65535) arg_error();
            server_port = strtol(argv[arg_count+1], &str_ptr, 10);
        }
        else
        {
            if (strcmp(argv[arg_count], "-mount") == 0)
            {
                if (mkdir(argv[arg_count+1], 0755)!=0) ERROR("creating directory failed!")
                if (strlen(argv[arg_count+1])>sizeof(file_path)) arg_error();
                strcpy(file_path, argv[arg_count+1]);
            }
            else
            {
                arg_error();
            }
        }
    }
    CHECK(chdir(file_path), "changing to assigned directory")
    DEBUG("using port:%d, using folder:%s\n", server_port, file_path);
    /*Open a socket and start to listen*/
    memset((void*) &client_addr, 0, sizeof(client_addr));
    memset((void*) &server_addr, 0, sizeof(server_addr));
    client_addr_len = sizeof(client_addr);
    CHECK(gethostname(hostname, 32), "getting host name")
    CHECK(server_socket = socket(AF_INET, SOCK_STREAM, 0), "opening server socket")
    server_addr.sin_port = htons(server_port);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(host2IpAddr(hostname));
    CHECK_N(bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)), "binding server socket")
    CHECK_N(listen(server_socket, QUEUE_LN), "start listening")
    while (1)
    {
        /*if (getch() == 'q') break;*/
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, (socklen_t*) &client_addr_len);
        if (client_socket < 0)
        {
            DEBUG("Error: client - accept failed, error %d\n", errno)
            perror("Error means:");
        }else
        {
            DEBUG("Connection accepted:%d", client_socket)
            if (pthread_create(&session_thread, NULL, (void*) &handle_client, (void *)client_socket) < 0)
            {
                DEBUG("Error in creating a new thread")
                break;
            }
        }
    }
    DEBUG("Closing server socket")
    close(server_socket);
    return EXIT_SUCCESS;
}
