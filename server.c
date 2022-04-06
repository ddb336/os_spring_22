#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>

#define PORT 5100
#define BUFF_SIZE 1024

#define SHELL_MAX_ARGS 64
#define SHELL_TOKEN_DELIMITERS " \t\r\n\a"

void *shell(void *);

char **parse_args(char *line, char** args);

#define NUM_SHELL_FUNCS 2

int shell_cd(char **args);
int shell_exit(char **args);

const char *shell_func_names[] = {"cd", "exit"};
int (*shell_funcs[])(char **) =
{
    &shell_cd,
    &shell_exit
};

/* ~ ~ ~ ~ ~ ~ ~ ~ ~ */

/* --- FUNCTION DEFINITIONS --- */

char *read_line();
int exec_func(char **args);
int exec_args(char **args);
int exec_comp(char **args, char **targs);

int main()
{
        // Here we create the socket
        int server_fd, new_socket;
        struct sockaddr_in address;
        int addrlen = sizeof(address);

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
                perror("socket failed");
                exit(EXIT_FAILURE);
        }

        // Here we bind to the socket
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
                perror("bind failed");
                exit(EXIT_FAILURE);
        }

        while (1)
        {       
                // Listen for connections on the port
                if (listen(server_fd, 3) < 0)
                {
                        perror("listen");
                        exit(EXIT_FAILURE);
                }
                // Accept connections on the port
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                         (socklen_t *)&addrlen)) < 0)
                {
                        perror("accept");
                        exit(EXIT_FAILURE);
                }
                // Create a new thread and run it with the ThreadRun function
                pthread_t th;
                pthread_create(&th, NULL, shell, &new_socket);
        }
        close(server_fd);
        return 0;
}

// Threadrun function
void *shell(void *socket)
{
        // Save the socket as an int
        int *sock = (int *)socket;
        int s = *sock;

        int status = 1;

        char** args = malloc(SHELL_MAX_ARGS * sizeof(char *));;
        char recv_buffer[BUFF_SIZE];

        int stin = dup(STDIN_FILENO); //save stdin
        int sout = dup(STDOUT_FILENO); //save stdout   

        int send_pipe[2];
        if(pipe(send_pipe) != 0 ) {
            fprintf(stderr, "Out pipe error\n");
            exit(EXIT_FAILURE);
        }

        char send_buffer[BUFF_SIZE] = {0};

        do
        {
            // pid_t pid = fork();
            // if(pid>0)

            dup2(send_pipe[1], STDOUT_FILENO); /* redirect stdout to the pipe */
            dup2(send_pipe[1], STDERR_FILENO);

            // Receive message from child
            memset(recv_buffer,0,strlen(recv_buffer));

            recv(s, recv_buffer, sizeof(recv_buffer), 0);

            if (strlen(recv_buffer) == 0) {
                status = 0;
                continue;
            }

            parse_args(recv_buffer, args);

            status = exec_func(args);

            memset(send_buffer,0,BUFF_SIZE);
            write(send_pipe[1], "\n", sizeof(char)*2);

            read(send_pipe[0], send_buffer, BUFF_SIZE);

            send(s, send_buffer, strlen(send_buffer), 0);

            for (size_t i = 0; i < SHELL_MAX_ARGS; i++)
            {
                if (args[i]) {
                    memset(args[i],0,strlen(args[i]));
                    args[i] = NULL;
                }
            }

        } while (status);

        return NULL;
}

char **parse_args(char *line, char** args)
{
    int pos = 0;

    char *token;

    if (!args)
    {
        fprintf(stderr, "Parse args error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, SHELL_TOKEN_DELIMITERS); // get the first arg

    // Go through the next args until you reach a NULL token
    while (token != NULL)
    {
        args[pos] = token; //add the token to the array of tokens
        pos++;

        token = strtok(NULL, SHELL_TOKEN_DELIMITERS); //and fetch the next one
    }

    //set the final token to NULL to allow exec to work correctly
    args[pos] = NULL;
    return args;
}

int exec_func(char **args)
{
    int j=0;
    while(args[j]!=NULL) j++; //start from end in outermost parent
    for(j--; j>=0; j--)
    {
        if (strcmp(args[j], "|") == 0) //if it is a composite pipe
        {
            int bufsize = SHELL_MAX_ARGS;
            memset(args[j],0,strlen(args[j]));
            args[j]=NULL;
            // create temp version of args to allow use of last
            // given args after comp handling
            char** targs= malloc(bufsize * sizeof(char *));
            int i=1;
            while(args[j+i]!=NULL)
            {
                targs[i-1] = args[j+i];
                i++;
            }
            targs[i-1]=NULL; //adding NULL to end of targs
            if(!exec_comp(args,targs)) //send to composite handling
                return 0; //if child exits, the whole thing exits
            args = targs; //args to be exec'd are the args after the ones that have been executed so far
            break;
        }
        // Output redirection to a file
        else if (strcmp(args[j], ">") == 0) {
            if (!args[j+1]) {
                printf("No file name given\n");
                return 1;
            } else {
                memset(args[j],0,strlen(args[j]));
                args[j]=NULL;
                // Reroute stdout to the file to read from
                if (!freopen(args[j+1], "w", stdout)) {
                    perror("FILE");
                    exit(EXIT_FAILURE);
                }
            }
        }
        // Input redirection from a file
        else if (strcmp(args[j], "<") == 0) {
            if (!args[j+1]) {
                printf("No file name given\n");
                return 1;
            } else {
                // Check that the file exists using access function
                if (access(args[j+1], F_OK) == 0) {
                    memset(args[j],0,strlen(args[j]));
                    args[j]=NULL;
                    // Reroute stdin to the file to read from
                    if (!freopen(args[j+1], "r", stdin)) {
                        perror("FILE");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    perror("FILE");
                    return 1;
                }
            }
        }
        // Output redirection to a file (with appending)
        else if (strcmp(args[j], ">>") == 0) {
            if (!args[j+1]) {
                printf("No file name given\n");
                return 1;
            } else {
                memset(args[j],0,strlen(args[j]));
                args[j]=NULL;
                // Reroute stdout to the file to read from
                if (!freopen(args[j+1], "a+", stdout)) {
                    perror("FILE");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    for (size_t i = 0; i < NUM_SHELL_FUNCS; i++) //look through the given special cases
    {
        if (strcmp(args[0], shell_func_names[i]) == 0) //if it is a special case
        {
            return (*shell_funcs[i])(args); //execute it separately
        }
    }

    return exec_args(args); //else execute it normally
}

int exec_comp(char **args, char **targs)
{
    int status;
    int fd[2];
    if(pipe(fd)==-1) //create pipe
    {
      perror("PIPE");
      exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    switch (pid)
    {
        case -1:
            // On error fork() returns -1.
            perror("FORK");
            exit(EXIT_FAILURE);
        case 0:
            // Child
            close(1); //close stdout
            close(fd[0]);
            dup(fd[1]); //dup into fdin
            close(fd[1]);
            if(!exec_func(args))//recursive call to check args up to the pending ones
                exit(1); //if exit, exit all
            exit(0);
        default:
            // Parent
            wait(&status);
            close(0); //close stdin
            close(fd[1]);
            dup(fd[0]); //dup into fdout
            close(fd[0]);
    }
    return (!status);
}

int exec_args(char **args) //normal execution
{
    int status;
    pid_t pid = fork();

    switch (pid)
    {
        case -1:
            // On error fork() returns -1.
            perror("FORK");
            exit(EXIT_FAILURE);
        case 0:
            // Child
            if (execvp(args[0], args) == -1)
            {
                perror("COMMAND ERROR");
            }
            exit(EXIT_FAILURE);
        default:
            // Parent
            wait(&status);
    }
    return 1;
}

int shell_cd(char **args) //special case: cd
{
    if (args[1] == NULL) {
        if (chdir(getenv("HOME")) != 0) {
            perror("cd error");
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd error");
        }
    }
    return 1;
}

int shell_exit(char **args) //special case: exit
{
    return 0; //return 0 as status
}
