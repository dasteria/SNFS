run make to generate three executables: serverSNFS clientapp throughput, and one shared library file libclientSNFS.so.
*******************************************************************************
serverSNFS:
a multithreaded server receives commands from clients and performs file operations (read, write, file status).
Usage serverSNFS <­-port port#> <-­mount filepath>.
for example: serverSNFS ­-port 12345 -­mount /tmp/fs416.

clientapp:
client application performs some operations.
Usage clientapp Usage clientapp <serverIP> <­port#> <filename>.
for example: clientapp cd.cs.rutgers.edu 10889 file.in.
The program will print the status of two files accessed on the server.

throughput:
fork a few number of clients (less than 200) to perform file operations.
Usage throughput <serverIP> <­port#> <client app number>.
for example: throughput ­cd.cs.rutgers.edu 10889 20.
The number of clients should be less than 200.
The program will print a list of pid and total number of CPU cylces for executing 10 client APIs.
