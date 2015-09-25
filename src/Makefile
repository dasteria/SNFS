default:
	gcc -g serverSNFS.c -lpthread -o serverSNFS
	gcc -c -fpic clientSNFS.c
	gcc -shared -o libclientSNFS.so clientSNFS.o
	gcc -g clientSNFS.c clientapp.c -o clientapp
	gcc -g clientSNFS.c throughput.c -o throughput
