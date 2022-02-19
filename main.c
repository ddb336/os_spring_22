#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

    do {
        printf("[om]> ");
        line = read_line();
        args = parse_args(line);
        status = exec_func(args);
    } while (status);
}

char *read_line()
{
    char *line = NULL;
    size_t buff_size = 0;

    if (getline(&line, &buff_size, stdin) == -1)
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
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "Parse args error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, SHELL_TOKEN_DELIMITERS);

    while (token != NULL)
    {
        tokens[pos] = token;
        pos++;

        token = strtok(NULL, SHELL_TOKEN_DELIMITERS);
    }

    tokens[pos] = NULL;
    return tokens;
}

int exec_func(char **args) 
{
    for (size_t i = 0; i < NUM_SHELL_FUNCS; i++)
    {
        if (strcmp(args[0], shell_func_names[i]) == 0) 
        {
            return (*shell_funcs[i])(args);
        }
    }

    return exec_args(args);
}

int exec_args(char **args)
{
    int status;
    pid_t pid = fork();

    switch (pid)
    {
        case -1:
            // On error fork() returns -1.
            perror("Fork failed\n");
            exit(EXIT_FAILURE);
        case 0:
            // Child
            if (execvp(args[0], args) == -1)
            {
                perror("Command failed.\n");
            }
            exit(EXIT_FAILURE);
        default:
            // Parent
            wait(&status);
    }
    return 1;
}

int shell_cd(char **args) 
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

int shell_exit(char **args)
{
    return 0;
}
