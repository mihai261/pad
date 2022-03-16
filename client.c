/**
 *  1. create socket
 *  2. connect to server
 *  3. ask for a file
 *  4. receive reply from server. does the requested file exist?
 *      - if the file does not exist, a message header with size == 0 is received
 *		- if the file exists, a message header with size == filesize is received
 *  5. if it exists, receive it
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
#include <errno.h>
#include "message.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define DIVISOR 32

#define PRINT_USAGE()   fprintf(stderr, "Incorrect usage.\n");    \
                        fprintf(stderr, "client FILE\n");

/*
 * Sets up the socket and connects to the server.
 * Returns 0 on success, -1 on error.
 */
int init_and_connect()
{
    // sockaddr_in is used for ipv4 sockets
    // zero all bytes of server_addr
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(struct sockaddr_in));

    // open socket
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1)
	{
		perror("Error opening socket");
		return -1;
	}

    // set server port and ip address
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	if(inet_aton(SERVER_IP, &server_addr.sin_addr) == 0)
	{
		fprintf(stderr, "Error interpreting ip address");
		close(socket_fd);
		return -1;
	}

    // connecting client to server
    socklen_t server_addr_len = sizeof(server_addr);
    if (connect(socket_fd, (struct sockaddr*) &server_addr, server_addr_len) == -1)
    {
        perror("Failed to connect to server");
        close(socket_fd);
        return -1;
    }
    printf("Connection established!\n");

    return socket_fd;
}

/*
 * Sends a request message to the server.
 * Message = header + name for requested file.
 * Returns 0 on success, -1 on error.
 */
int request_file(int socket_fd, const char* filename)
{
    // build header for request message
    message_header header;
    header.message_type = 'f';
    header.message_size = strlen(filename) + 1;

    // send header
    if (write(socket_fd, (void*) &header, sizeof(message_header)) == -1)
    {
        perror("Error sending header for request message: ");
        return -1;
    }

    // send filename
    if (write(socket_fd, (void*) filename, strlen(filename) + 1) == -1)
    {
        perror("Error sending file request message: ");
        return -1;
    }

    return 0;
}

/*
 * Reads the initial reply of the server.
 * A return value of 0 means the file doesn't exist on the server machine.
 * Any other value can be interpreted as the size of the requested file, in bytes.
 * A return value of -1 may signal an error, or an inappropriate reply (not file transfer).
 */
int await_initial_server_reply(int socket_fd)
{
    // reading server reply
    message_header header;
	if (read(socket_fd, (void*) &header, sizeof(message_header)) == -1)
	{
		perror("Error receiving reply from server");
		return -1;
	}

    // if the reply header is not tagged with a 'f', the process ends
    if (header.message_type != 'f')
    {
        fprintf(stderr, "Reply not for file transfer\n");
        return -1;
    }

    return header.message_size;
}

/*
 * Receives the file segments from the socket and copies them in an output file
 * 
 */
int receive_file(int socket_fd, const char* filename, size_t filesize)
{
    size_t received_size = 0;
    message_header header;
    char* buffer = NULL; 
    char* aux = NULL;

    // creating an appropiate name for the received file (strlen())
    size_t filename_len = strlen("received_") + strlen(filename) + 1;
    char* filename_buffer = (char*) malloc(filename_len * sizeof(char));
    if (filename_buffer == NULL)
    {
        errno = ENOMEM;
        perror("Could not create buffer for filename");
        return -1;
    }
    sprintf(filename_buffer, "received_%s", filename);

    // open output file
    FILE* file = fopen(filename_buffer, "w");
    if (file == NULL)
    {
        perror("Could not open output file");
        free(filename_buffer);
        return -1;
    }

    // read file segments from the socket until I will have read the size of the entire file
    while (received_size < filesize)
    {
        // read the header for the current message
        if (read(socket_fd, &header, sizeof(message_header)) == -1)
        {
            perror("Error reading header");
            fclose(file);
            free(buffer);
            free(filename_buffer);
            return -1;
        }

        // adjust buffer for storing file segment based on the size of the current message
        aux = (char*) realloc(buffer, header.message_size * sizeof(char));
        if (aux == NULL)
        {
            errno = ENOMEM;
            perror("Failed to adjust buffer");
            fclose(file);
            free(buffer);
            free(filename_buffer);
            return -1;
        }
        buffer = aux;

        // read the file segment from the socket into the buffer
        ssize_t read_size = 0;
        if ((read_size = read(socket_fd, buffer, header.message_size+1)) == -1)
        {
            perror("Error reading file segment from socket");
            fclose(file);
            free(buffer);
            free(filename_buffer);
            return -1;
        }

        //checksum on received segment
        int checksum = 0;
		for(int i=0; i<read_size-1; i++){
			checksum += (int) buffer[i];
		}
		checksum = checksum % DIVISOR;

		if(checksum != (int) buffer[read_size-1]){
            fprintf(stderr, "Wrong checksum!\n");
            fclose(file);
            free(buffer);
            remove(filename_buffer);
            free(filename_buffer);
            return -1;
        }
        
        // write the file segment in the output file
        if (fwrite(buffer, sizeof(char), read_size-1, file) != read_size-1)
        {
            fprintf(stderr, "Not enough bytes were written in the output file.\n");
            fclose(file);
            free(buffer);
            free(filename_buffer);
            return -1;
        }

        // increment number of transferred bytes
        received_size += read_size - 1;
    }

    fclose(file);
    free(buffer);
    free(filename_buffer);
    return 0;
}

int main(int argc, char* argv[])
{
    // parse requested file name from command line arguments
    if (argc < 2)
    {
        PRINT_USAGE();
        exit(EXIT_FAILURE);
    }
    char* requested_filename = argv[1];

    // init the socket and connect to the server
    int socket_fd = init_and_connect();
    if (socket_fd == -1)
    {
        exit(EXIT_FAILURE);
    }

    // request the file from the server
    if (request_file(socket_fd, requested_filename) == -1)
    {
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // receive reply from server. does the file exist or not? if yes, receive it
    int filesize = await_initial_server_reply(socket_fd);
    if (filesize == -1)
    {
        // error
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    else if (filesize == 0)
    {
        // file does not exist
        printf("File does not exist on server machine.\n");
    }
    else
    {
        // ask for permission to allocate memory
        printf("After this operation, %d bytes of additional disk space will be used.\nDo you want to continue? [y/n]", filesize);
        char response;
        scanf("%c", &response);

        if(response == 'Y' || response == 'y'){
            // file exists, proceed with receiving it
            if (receive_file(socket_fd, requested_filename, filesize) == -1)
            {
                fprintf(stderr, "File not transmitted properly.\n");
            }
            else
            {
                printf("File received!\n");
            }
        }
    }

	close(socket_fd);
	return 0;
}
