#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 

#define PORT 5100 
#define BUFF_SIZE 1024

#define SHELL_MAX_ARGS 64
#define SHELL_TOKEN_DELIMITERS " \t\r\n\a"

int getSocket();
void shell(int socket);
char *read_line();
char **parse_args(char *line);
int get_exec_result(char **args, int sock);
void print_result(char* buffer);

int main()
{
    int sock = getSocket();
    shell(sock);
    return 0;
}

int getSocket() 
{
    // Here we create the socket
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    char buffer[BUFF_SIZE] = {0}; 
    
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
    
    return sock;
}

void shell(int sock)
{
    char *line;
    char **args;
    int status = 1;
    char buffer[BUFF_SIZE] = {0}; 

    int stin = dup(0); //save stdin
    int sout = dup(1); //save stdout


    do { //shell loop
        

        // TODO: Send this over to server
        dup2(stin,0); //reset input to stdin 
        dup2(sout,1); //reset output to stdout 


        printf("[om]> ");
        line = read_line();
        if (!strcmp(line,"\n")) continue;
        if (!strcmp(line,"exit")) break;
        status = get_exec_result(line, buffer, sock);
        print_result(buffer);
    } while (status); //turns false with exit
}

char* read_line()
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

int get_exec_result(char* line, char* buffer, int sock) 
{
    send(sock, *line, strlen(*line), 0);
    read(sock, buffer, BUFF_SIZE);
    return atoi(buffer[0]);
}

void print_result(char* buffer)
{
    printf("%s", buffer+1);
}