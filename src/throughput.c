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

#define CLOCK_RATE 2394.468
#define SERVER_LOAD 200
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

// Start of rdstc clock reading
static inline unsigned long long rdtsc_begin(void)
{
  unsigned cycles_high,cycles_low;

 __asm__ __volatile__
(
"CPUID\n\t"/*serialize*/
"RDTSC\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t": "=r" (cycles_high), "=r"
(cycles_low):: "%rax", "%rbx", "%rcx", "%rdx"); 
  return ( (unsigned long long)cycles_low)|( ((unsigned long long)cycles_high)<<32);
}

// End of rdstc clock reading
static inline unsigned long long rdtsc_end(void)
{

  unsigned cycles_high1,cycles_low1;
 __asm__ __volatile__
(

"RDTSCP\n\t"/*read the clock*/
"mov %%edx, %0\n\t"
"mov %%eax, %1\n\t"
"CPUID\n\t": "=r" (cycles_high1), "=r"
(cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx");
  return ( (unsigned long long)cycles_low1)|( ((unsigned long long)cycles_high1)<<32 );
}

/**
 * Open a new client application with basic API's:
 * 		setServer();
 * 		openFile(), writeFile(), readFile(), statFile(), closeFile();
 * 
 * @param hostname: hostname used to connect to server 
 * @param port_id: port number used to connect to server
 * @param filename: string of filename for creating files
 * 
 * @return total amount of CPU ticks elapsed between client API calls
 */

static long long openClient(char hostname[],int port_id,char filename[])
{
	unsigned long long tickStart,tickEnd,tickElapsed;
		    
	/**
	 * Sample file name f_one :  file000.in
	 * Sample file name f_two :  rev_file000.in
	 */
	char f_one[sizeof "file1000.in"];
	strcpy(f_one,"in");
	char f_two[sizeof "rev_file1000.in"];
	strcpy(f_two,"rev_");
	strcat(f_two,"out");
	int apiCounter = 10;
    int fd_one, fd_two;
    int rev_counter;
    char buffer[BUFFER_SIZE+1];
    char rev_buffer[BUFFER_SIZE+1];
    struct fileStat st_one,st_two;
    
    /**
     * Prepare two strings of size BUFFER_SIZE=1024 for our test.
     */
    strcpy(buffer, filename);
    for(rev_counter=strlen(filename);rev_counter<BUFFER_SIZE;rev_counter++)
    {
        buffer[rev_counter] = 'A';
    }
    buffer[BUFFER_SIZE] = '\0';
    for(rev_counter=0;rev_counter<BUFFER_SIZE;rev_counter++)
    {
        rev_buffer[BUFFER_SIZE-rev_counter-1] = buffer[rev_counter];
    }
    rev_buffer[BUFFER_SIZE] = '\0';
    /**
     * Read time counter here
     */
	tickStart = rdtsc_begin();
    
    setServer(hostname, port_id);
    /**
     * Create, write and read the status of the 1st file "file000.in" from server
     */
    fd_one = openFile(f_one);
    CHECK(fd_one, "open/create file in server")
    CHECK(writeFile(fd_one, buffer), "write file to server")
    CHECK(statFile(fd_one, &st_one), "read file status from server")
    CHECK(readFile(fd_one, buffer), "read file from server")
    CHECK(closeFile(fd_one), "close file in server")
    /**
     * Create, write and read the status of the 2nd file "rev_file000.in" from server
     */
    fd_two = openFile(f_two);
    CHECK(fd_two, "open/create file in server");
    CHECK(writeFile(fd_two, rev_buffer), "write file to server");
    CHECK(statFile(fd_two, &st_two), "read file status from server");
    CHECK(closeFile(fd_two), "close file in server");

    /**
     * Read time counter, calculate ticks & time elapsed
     * and calculate server throughput
     */
	tickEnd = rdtsc_end();
	tickElapsed = (tickEnd- tickStart);
	printf("%d\tCycles spent : %llu\n",getpid(), tickElapsed);
	/*
   	printf("\nCycles spent : %llu\n",tickElapsed);
	printf("\nTime spent : %e (us)\n",(tickElapsed)/CLOCK_RATE);
	printf("\nTotal number of API calls completed : %d\n",apiCounter);
	printf("\nClient application throughput : %e\n",apiCounter/((tickElapsed*10^6)/CLOCK_RATE));
    printf("in file:%d, %s, %s, %s\n", st_one.size, st_one.birthtime, st_one.atime, st_one.mtime);
    printf("reversed file:%d, %s, %s, %s\n", st_two.size, st_two.birthtime, st_two.atime, st_two.mtime);
    */
	return apiCounter/((tickElapsed*10^6)/CLOCK_RATE);

}

void arg_error(void)
{
    fprintf(stderr, "Usage throughput <serverIP> <­port#> <forkCounter>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	// Counter to generate fork processes
	int forkCounter;
	char *hostname, *str_ptr;
	char fn[sizeof "file1000.in"];
	int port_id;
	// Counter to generate unique names for each client application
	int nameCounter;
	// Array of string to store generated file names, {null} initially
    char* nameArray[SERVER_LOAD] = {0};
    
    unsigned long long totalThroughput;
    
    /**
     * Process three parameters:
     * the server IP, the server port and server_load
     */
    if (argc != 4) arg_error();
    hostname = argv[1];
    if (strtol(argv[2], &str_ptr, 10) <= 0
            || strtol(argv[2], &str_ptr, 10) >65535) arg_error();
    port_id = strtol(argv[2], &str_ptr, 10);
    if (strtol(argv[3], &str_ptr, 10) <= 0
            || strtol(argv[3], &str_ptr, 10) >SERVER_LOAD) arg_error();
    forkCounter = strtol(argv[3], &str_ptr, 10);
    DEBUG("server:%s, port:%d, number of processes:%d", hostname, port_id, forkCounter);

	/**
	 * Generate unique file names and store in an array
	 * 

	for(nameCounter=0;nameCounter<SERVER_LOAD;nameCounter++)
	{
		char filename[sizeof "file1000.in"];
		sprintf(filename, "file%d.in", nameCounter);
		nameArray[nameCounter] = filename;
		//printf("\n%s",nameArray[counter]);
	}
	*/
	/**
	 * Fork client applications with same set of API calls
	 * and unique filename input.
	 * 
	 */
    int i;
    for (i = 0; i < forkCounter; i++)
    {
        int pid = fork();
        if (pid < 0) /* check for error      */
        {
            printf("Fork error.\n");
            exit(EXIT_FAILURE);
        }else
            if (pid == 0)
            {
                sprintf(fn, "file%03d.in", i+1);
                totalThroughput += openClient(hostname, port_id, fn);
                exit(0);
            }

    }
	printf("\nThroughput of the server is : %e per second\n",totalThroughput);
    return 0;
}
