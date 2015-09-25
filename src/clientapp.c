/*
 * clientapp.c
 *
 *  Created on: Nov 5, 2014
 *      Author: daxu
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "macro.h"

struct fileStat
{
    off_t size;                  /* total size, in bytes */
    char birthtime[DATE_LN];     /* time of creation */
    char atime[DATE_LN];         /* time of last access */
    char mtime[DATE_LN];         /* time of last modification */
};

extern void setServer(char *serverIP, int port);
extern int openFile(char *name);
extern int readFile(int fd, void *buf);
extern int writeFile(int fd, void *buf);
extern int statFile(int fd, struct fileStat *buf);
extern int closeFile(int fd);

void arg_error(void)
{
    fprintf(stderr, "Usage clientapp <serverIP> <­port#> <filename>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int port_id = PORT_ID;
    char *hostname, *f_one, *str_ptr;
    char f_two[] = "reverse.in";
    int fd_one, fd_two;
    int rev_counter;
    char buffer[BUFFER_SIZE+1]={0};
    char rev_buffer[BUFFER_SIZE+1]={0};
    struct fileStat st_one,st_two;

    /**
     * Process three parameters:
     * the server IP, the server port and a valid file name
     */
    if (argc != 4) arg_error();
    hostname = argv[1];
    if (strtol(argv[2], &str_ptr, 10) <= 0
            || strtol(argv[2], &str_ptr, 10) >65535) arg_error();
    port_id = strtol(argv[2], &str_ptr, 10);
    f_one = argv[3];
    DEBUG("server:%s, port:%d, file:%s\n", hostname, port_id, f_one);

    /**
     * Read from local file
     */
    fd_one = open(f_one, O_RDONLY, 755);
    CHECK(fd_one, "open local file")
    CHECK(read(fd_one, buffer, BUFFER_SIZE), "read local file")
    CHECK(close(fd_one), "close local file")

    /**
     * TODO:Add code to read time counter here
     */
    setServer(hostname, port_id);
    /**
     * Create, write and read the status of the 1st file "file.in" from server
     */
    fd_one = openFile(f_one);
    CHECK(fd_one, "open/create file in server")
    CHECK(writeFile(fd_one, buffer), "write file to server")
    CHECK(statFile(fd_one, &st_one), "read file status from server")
    int len;
    CHECK(len = readFile(fd_one, buffer), "read file from server")
    CHECK(closeFile(fd_one), "close file in server")
    /**
     * Reverse buffered content from "file.in", store result in rev_buffer
     */
    for(rev_counter=0;rev_counter<len;rev_counter++)
    {
        rev_buffer[len-rev_counter-1] = buffer[rev_counter];
    }
    rev_buffer[len] = '\0';
    /**
     * Create, write and read the status of the 2nd file "reverse.in" from server
     */
    fd_two = openFile(f_two);
    CHECK(fd_two, "open/create file in server")
    CHECK(writeFile(fd_two, rev_buffer), "write file to server")
    CHECK(statFile(fd_two,&st_two), "read file status from server")
    CHECK(closeFile(fd_two), "close file in server")
    /**
     * TODO:Add code to read time counter here,
     * And calculate time passed.
     */
    printf("file.in:%d, %s, %s, %s\n", st_one.size, st_one.birthtime, st_one.atime, st_one.mtime);
    printf("reversed.in:%d, %s, %s, %s\n", st_two.size, st_two.birthtime, st_two.atime, st_two.mtime);
    /**
     * printf("time used by this client:...\n");
     */
    return 0;
}
