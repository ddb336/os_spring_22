#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

//commands available: echo, mv, cd, ls, pwd, cat, wc, uniq, sort, rm, touch, rmdir, mkdir, exit

#define SHELL_MAX_ARGS 32
#define SHELL_TOKEN_DELIMITERS " \t\r\n\a"

/* --- SHELL BUILTINS --- */

#define NUM_SHELL_FUNCS 2

int shell_cd(char **args);
int shell_exit(char **args);

char *shell_func_names[] = {"cd", "exit"};

int (*shell_funcs[])(char **) =
{
    &shell_cd,
    &shell_exit
};

/* ~ ~ ~ ~ ~ ~ ~ ~ ~ */

/* --- FUNCTION DEFINITIONS --- */

void shell();
char *read_line();
char **parse_args(char *line);
int exec_func(char **args);
int exec_args(char **args);
int exec_comp(char **args, char **targs);

/* ~ ~ ~ ~ ~ ~ ~ ~ ~ */

int main()
{
    shell();
    return EXIT_SUCCESS;
}

void shell()
{
    char *line;
    char **args;
    int status;
    int stin = dup(0); //save stdin
    int sout = dup(1); //save stdout
    do { //shell loop
        dup2(stin,0); //reset input to stdin
        dup2(sout,1); //reset output to stdout
        printf("[om]> ");
        line = read_line();
        args = parse_args(line);
        status = exec_func(args);
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
            perror("Read line error.");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

char **parse_args(char *line)
{
    int bufsize = SHELL_MAX_ARGS, pos = 0;
    char **tokens = malloc(bufsize * sizeof(char *)); //list of arguments to be returned
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "Parse args error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, SHELL_TOKEN_DELIMITERS); //get the first arg

    while (token != NULL) //go through the next args until you reach a NULL token
    {
        tokens[pos] = token; //add the token to the array of tokens
        pos++;

        token = strtok(NULL, SHELL_TOKEN_DELIMITERS); //and fetch the next one
    }

    tokens[pos] = NULL; //set the final token to NULL to allow exec to work correctly
    return tokens;
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
            args[j]=NULL;
            char** targs= malloc(bufsize * sizeof(char *)); //create temp version of args to allow use of last given args after comp handling
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
    int* fd = malloc(2*sizeof(int));
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
