/**
 * 	1. create socket
 *	2. bind and start listening
 *  3. accept connection
 *	4. wait for client to ask u for a file
 *  5. check if that file exists and reply to the client
 *		- if the file does not exist, a message header with size == 0 is sent
 *		- if the file exists, a message header with size == filesize is sent
 *  6. if it exists, send it (not implemented)
 */


#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "message.h"

#define IP "127.0.0.1"
#define PORT 8080

int init_server()
{
	// sockaddr_in is used for ipv4 sockets
	struct sockaddr_in addr;
	bzero(&addr, sizeof(struct sockaddr_in)); // < guaranteed to work

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
	{
		perror("error opening socket: ");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	if(inet_aton(IP, &addr.sin_addr) == 0)
	{
		fprintf(stderr, "error converting address");
		close(sd);
		return -1;
	}

	if ((bind(sd, (struct sockaddr*) &addr, sizeof(struct sockaddr))) != 0)
	{
		perror("bind failed: ");
		close(sd);
		return -1;
	}

	return sd;
}

int await_client_connection(int socket_fd)
{
	struct sockaddr_in client_addr;
	bzero(&client_addr, sizeof(struct sockaddr_in));

	// starting the listening process for inbound connections
	if (listen(socket_fd, 5) == -1)
	{
		perror("error starting the listening: ");
		close(socket_fd);
		return -1;
	}

	printf("waiting...\n");

	socklen_t client_addr_len = sizeof(client_addr_len);
	int csd; // < client socket descriptor
	csd = accept(socket_fd, (struct sockaddr*) &client_addr, &client_addr_len);
	if (csd == -1)
	{
		perror("error establishing connection");
		close(socket_fd);
		return -1;
	}
	printf("connection established!\n");

	return csd;
}

char* accept_file_request(int socket_fd)
{
	// read header
	message_header header;
	if (read(socket_fd, (void*) &header, sizeof(message_header)) == -1)
	{
		perror("Error receiving file request header: ");
		return NULL;
	}

	// make space for filename
	char* filename = (char*) malloc(header.message_size * sizeof(char));
	if (filename == NULL)
	{
		errno = ENOMEM;
		perror("Error making space for file name:");
		return NULL;
	}

	// read filename
	if (read(socket_fd, (void*) filename, header.message_size) == -1)
	{
		perror("Error reading the filename from socket: ");
		return NULL;
	}

	return filename;
}

int check_if_file_exist(int socket_fd, const char* filename)
{
	message_header header;
	header.message_type = 'f';

	// checking if file exists with stat instead of access because we'll use
	// st_size afterwards
	struct stat statbuf;
	int status = stat(filename, &statbuf);
	if (status == -1 && errno == ENOENT)
	{
		// file doesn't exist, inform client
		// we send a header with message_type == 0 to signal that
		// there is no file
		header.message_size = 0;
		printf("file does not exist\n");
	}
	else if (status == -1)
	{
		// another error occured, just exit and hope for the best
		return -1;
	}
	else
	{
		// file exists, inform client you will start sending the file
		// we send a header with message_type == file size in B to signal
		// that the file exists
		header.message_size = statbuf.st_size;
	}

	if (write(socket_fd, (void*) &header, sizeof(message_header)) == -1)
	{
		perror("Error informing client: ");
		return -1;
	}
	return header.message_size;
}

int main(int argc, char* argv[])
{

	int socket_fd = init_server();
	if (socket_fd == -1)
	{
		exit(EXIT_FAILURE);
	}

	int client_socket_fd = await_client_connection(socket_fd);
	if (client_socket_fd == -1)
	{
		exit(EXIT_FAILURE);
	}

	// see what file the client needs
	char* requested_filename = accept_file_request(client_socket_fd);
	if (requested_filename == NULL)
	{
		close(client_socket_fd);
		close(socket_fd);
		exit(EXIT_FAILURE);
	}

	printf("requested file: %s\n", requested_filename);

	int ret_val = check_if_file_exist(client_socket_fd, requested_filename);
	if (ret_val == -1)
	{
		free(requested_filename);
		close(client_socket_fd);
		close(socket_fd);
		exit(EXIT_FAILURE);
	}
	if (ret_val == 0)
	{
		// file does not exist, do nothing?
	}
	else
	{
		// file exists, call sending function
	}

	free(requested_filename);
	close(client_socket_fd);
	close(socket_fd);
	return 0;
}