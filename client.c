/**
 *  1. create socket
 *  2. connect to server
 *  3. ask for a file
 *  4. receive reply from server. does the requested file exist?
 *      - if the file does not exist, a message header with size == 0 is received
 *		- if the file exists, a message header with size == filesize is received
 *  5. if it exists, receive it (not implemented)
 */


#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include "message.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

#define PRINT_USAGE()   fprintf(stderr, "Incorrect usage.\n");    \
                        fprintf(stderr, "client FILE\n");

int init_and_connect()
{
    // sockaddr_in is used for ipv4 sockets
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(struct sockaddr_in)); // < guaranteed to work

    // opening socket
	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
	{
		perror("error opening socket: ");
		return -1;
	}

    // creating entity for the server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	if(inet_aton(SERVER_IP, &server_addr.sin_addr) == 0)
	{
		fprintf(stderr, "error converting address");
		close(sd);
		return -1;
	}

    // connecting client to server
    socklen_t server_addr_len = sizeof(server_addr); // < astea sunt necesare deoarece variabilele sockaddr se castuie in functie de tipul de stiva de protoc folosite(?)
    if (connect(sd, (struct sockaddr*) &server_addr, server_addr_len) == -1)
    {
        perror("Failed to connect to server: ");
        close(sd);
        return -1;
    }
    printf("connection established!\n");

    return sd;
}

int request_file(int socket_fd, const char* filename)
{
    // build header
    message_header header;
    header.message_type = 'f';
    header.message_size = strlen(filename) + 1;

    // send header
    if (write(socket_fd, (void*) &header, sizeof(message_header)) == -1)
    {
        perror("Error sending file name meta: ");
        return -1;
    }

    // send filename
    if (write(socket_fd, (void*) filename, strlen(filename) + 1) == -1)
    {
        perror("Error sending file name: ");
        return -1;
    }

    return 0;
}

int await_initial_server_reply(int socket_fd)
{
    message_header header;
	if (read(socket_fd, (void*) &header, sizeof(message_header)) == -1)
	{
		perror("Error receiving reply from server: ");
		return -1;
	}

    return header.message_size;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        PRINT_USAGE();
        exit(EXIT_FAILURE);
    }
    char* requested_file_name = argv[1];

    int socket_fd = init_and_connect();
    if (socket_fd == -1)
    {
        exit(EXIT_FAILURE);
    }

    if (request_file(socket_fd, requested_file_name) == -1)
    {
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    int ret_val = await_initial_server_reply(socket_fd);
    if (ret_val == -1)
    {
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    else if (ret_val == 0)
    {
        printf("file does not exist on server machine.\n");
    }
    else
    {
        printf("file exists on server machine with size %d.\n", ret_val);
    }

	close(socket_fd);
	return 0;
}
