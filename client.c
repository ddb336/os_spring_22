#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define PORT 5101
#define BUFF_SIZE 1024

void shell(int sock);
char *read_line();

int main(int argc, char const *argv[])
{
    // Here we create the socket
    int sock = 0, valread;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\nSocket creation error \n");
        return -1;
    }

    // Here we try to connect to the socket on a specific port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    shell(sock);

    return 0;
}

void shell(int sock)
{
    char *line;
    char **args;
    int status = 1;
    char recv_buffer[BUFF_SIZE] = {0};

    do { 
        fflush(stdin);
        memset(recv_buffer,0,BUFF_SIZE);

        printf("[om]> ");
        line = read_line();
        if (!strcmp(line,"\n")) continue;

        send(sock, line, strlen(line), 0);

        if (!strcmp(line,"exit\n")) break;
        
        recv(sock, recv_buffer, BUFF_SIZE, 0);

        recv_buffer[strlen(recv_buffer)-1] = '\0';

        printf("%s", recv_buffer);
        
    } while (status); //turns false with exit
}

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

