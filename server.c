#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>    

// The port and buffer size defined
#define PORT 5100
#define BUFF_SIZE 4096

// Maximum number of args in the shell, and command delimiters
#define SHELL_MAX_ARGS 64
#define SHELL_TOKEN_DELIMITERS " \t\r\n\a"

/* --- BUILTIN FUNCTIONS --- */

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

void* schedule(void* saved_std);

/* --- JOB STRUCT --- */

int QUANTUM = 100;

sem_t job_semaphore;    

struct Job {
    char **args;
    int n;
    sem_t finished;
    sem_t linked;
    struct Job* next;
    int executing;
    bool is_done;
};

struct Queue {
    struct Job* head;
    struct Job* min;
} job_queue;

struct saved_std {
    int saved_stdout;
    int saved_stderr;
};

void quantum_wait(struct Job * job_to_do);

void exec_job(struct Job* job_to_do, char* sem_name);

int msleep(long msec);
void * program(void * job_to_do);

/* --- FUNCTION DEFINITIONS --- */

void *shell(void *);
char **parse_args(char *line, char** args);
char *read_line();
int exec_func(char **args);
int exec_args(char **args);
int exec_comp(char **args, char **targs);

/* ~ ~ ~ ~ ~ ~ ~ ~ ~ */

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
        int optval = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
                perror("bind failed");
                exit(EXIT_FAILURE);
        }

        sem_init(&job_semaphore, 0, 0);

        struct saved_std schedule_std;
        schedule_std.saved_stderr = dup(STDOUT_FILENO);
        schedule_std.saved_stderr = dup(STDERR_FILENO);
        
        pthread_t scheduler;
        pthread_create(&scheduler, NULL, schedule, &schedule_std);

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
                // Create a new thread and run it with the shell function
                pthread_t th;
                pthread_create(&th, NULL, shell, &new_socket);
        }
        close(server_fd);
        return 0;
}

// Shell function
void *shell(void *socket)
{
        // Save the socket as an int
        int *sock = (int *)socket;
        int s = *sock;

        int status = 1;

        // Create the args
        char** args = malloc(SHELL_MAX_ARGS * sizeof(char *));;
        char recv_buffer[BUFF_SIZE];

        // Here we create a pipe to save the stderr and stdout
        int send_pipe[2];
        if(pipe(send_pipe) != 0 ) {
            fprintf(stderr, "Out pipe error\n");
            exit(EXIT_FAILURE);
        }

        char send_buffer[BUFF_SIZE] = {0};

        do
        {
            // Here we sup the stdout and stderr into the send pipe
            dup2(send_pipe[1], STDOUT_FILENO);
            // dup2(send_pipe[1], STDERR_FILENO);

            // Receive message from child
            memset(recv_buffer,0,strlen(recv_buffer));
            recv(s, recv_buffer, sizeof(recv_buffer), 0);

            // If empty message, continue
            if (strlen(recv_buffer) == 0) {
                status = 0;
                continue;
            }

            // Parse the arguments and execute them
            parse_args(recv_buffer, args);
            status = exec_func(args);

            // Empty the buffer and write something into pipe (so that client
            // doesnt end up waiting if nothing is sent to the client)
            memset(send_buffer,0,BUFF_SIZE);
            write(send_pipe[1], " ", sizeof(char)*2);

            // Read from pipe to buffer
            read(send_pipe[0], send_buffer, BUFF_SIZE);

            // Send the buffer
            send(s, send_buffer, strlen(send_buffer), 0);

            // Empty the args
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
            if(args[j+1]==NULL) //if it is a bad request for piping, ignore
            {
                fprintf(stderr,"PIPE: no arguments after pipe\n");
                return 1;
            }
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
                fprintf(stderr,"FILE: No file name given\n");
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
                fprintf(stderr,"FILE: No file name given\n");
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
                fprintf(stderr,"FILE: No file name given\n");
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
    if (!strcmp(args[0], "./test_prog")) {
        struct Job new_job;
        new_job.args = args;
        new_job.next = job_queue.head;
        job_queue.head = &new_job;
        if (args[1] == NULL) {
            new_job.n = 1;
        } else {
            new_job.n = atoi(args[1]);
        }
        new_job.is_done = false;
        new_job.executing = false;

        sem_init(&new_job.finished, 0, 0);

        sem_post(&job_semaphore); 
        sem_wait(&new_job.finished);
        return 1;
    }

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

void *schedule(void* saved_std)
{

    struct saved_std *my_std = (struct saved_std*)saved_std;

    int ctr = 0;
    while (1) {
        ctr++;
        sem_wait(&job_semaphore); 
        
        dup2(my_std->saved_stderr, STDERR_FILENO);
        dup2(my_std->saved_stdout, STDOUT_FILENO);

        fprintf(stderr, "Back here %d\n", ctr);
        struct Job *current = job_queue.head;
        struct Job *job_to_do = job_queue.head;

        while (current != NULL) {
            if (current->n < job_to_do->n) {
                job_to_do = current;
            }
            current=current->next;
        }

        fprintf(stderr, "Starting Job %d\n", ctr);

        char sem_name[64];
        sprintf(sem_name, "%d", ctr);

        if (!job_to_do->executing) {
            fprintf(stderr,"new job...\n");
            exec_job(job_to_do, sem_name);
        } else {
            fprintf(stderr,"job executing...\n");
            sem_post(&job_to_do->linked);
            job_to_do->executing++;
        }
        quantum_wait(job_to_do);
    }
    return NULL;
}

void exec_job(struct Job* job_to_do, char *sem_name)
{   
    sem_init(&job_to_do->linked, 0, 1);

    pthread_t prog_thread;
    pthread_create(&prog_thread, NULL, program, (void*)job_to_do);

    job_to_do->executing = true;
}

void quantum_wait(struct Job* job)
{
    struct timeval t0;
    struct timeval t1;
    gettimeofday(&t0, 0);

    while (!job->is_done) {
        gettimeofday(&t1, 0);
        long int t = (t1.tv_sec-t0.tv_sec)*1000000 + t1.tv_usec-t0.tv_usec;
        t = t/1000;
        if(t>QUANTUM*job->executing)
        {
            sem_wait(&job->linked);
            job->n -= QUANTUM*job->executing;
            sem_post(&job_semaphore);
            fprintf(stderr,"reached timeout!\n");
            return;
        }
    }

    sem_post(&job->finished);

    struct Job *current;

    if (job_queue.head == job) {
        job_queue.head = job_queue.head->next;
    } else {
        current = job_queue.head;
        while (current->next != job) {
            current = current->next;
        }
        current->next = job->next;
    }
}

void * program(void * job_to_do)
{
    struct Job* job = (struct Job*)job_to_do;
    int n = job->n;
    for (size_t i = 0; i < n; i++)
    {
        sem_wait(&job->linked);
        printf("%d\n", (int) i);
        msleep(100); // 100 milliseconds
        sem_post(&job->linked);
    }
    fprintf(stderr,"a\n");
    job->is_done = true;
    return NULL;
}

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}