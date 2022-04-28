#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define PORT 5100
#define BUFF_SIZE 1024

int get_socket();
void shell(int sock);
char *read_line();

/*
 * Daniel & Mo
 * Shell Project - Phase 2
 * Spring 2022
 */

// This is the main function. It creates the socket and then calls the 
// shell function using the socket.
int main()
{
    // Get the socket
    int sock = get_socket();
    // Run the shell with the socket
    shell(sock);
    return 0;
}

// This function gets the socket
int get_socket()
{
    int sock = 0, valread;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\nSocket creation error \n");
        exit(EXIT_FAILURE);
    }

    // Here we try to connect to the socket on a specific port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        exit(EXIT_FAILURE);
    }

    return sock;
}
    
// This is the shell function that loops, reads input from the user, and
// sends the input to the server. It then reads the response and prints it.
void shell(int sock)
{
    char *line;
    char **args;
    int status = 1;
    char recv_buffer[BUFF_SIZE] = {0};

    while (1) {
        // Clear stdin
        fflush(stdin);

        // Reset the buffer to empty
        memset(recv_buffer,0,BUFF_SIZE);

        // Print the prompt
        printf("[om]> ");

        // Read the line from the user
        line = read_line();

        // If the user hits enter, just continue
        if (!strcmp(line,"\n")) continue;

        // Send the data to the server
        send(sock, line, strlen(line), 0);

        // If the user chooses to exit, break the loop
        if (!strncmp(line, "exit", sizeof(char)*4)) break;
        
        // Receive response from server
        recv(sock, recv_buffer, BUFF_SIZE, 0);

        // Remove the final char from the response (it is a dummy char)
        recv_buffer[strlen(recv_buffer)-1] = '\0';

        // Print response
        printf("%s", recv_buffer);
    }
}

// This function reads input from the user
char *read_line()
{
    char *line = NULL;
    size_t buff_size = 0;

    if (getline(&line, &buff_size, stdin) == -1) //save the line and return it
    {
        if (feof(stdin))
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            perror("READLINE");
            return "";
        }
    }

    return line;
}

