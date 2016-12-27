//Sabrina Smith
//CMPS 360
//70.c
//Use of fork(2), wait(2), exit(2), open(2), close(2), write(2), sigprocmask(2), kill(2), sigsuspend(2), sigaction(2)
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#define BUFSIZE 50

/*GLOBAL VARIABLES*/
char mystr[BUFSIZE];
int logfd; 
char buf[BUFSIZE];
int status = 0;

/*struct sigaction{
    void        (*sa_handler)(int);
    void        (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t    sa_mask;
    int         sa_flags;
    void        (*sa_restorer)(void);
};*/

int fib(int n){
    if (n == 0) return 1;
    if (n == 1) return 1;
    return fib(n-1) + fib(n-2);
}

/* SIGUSR1 HANDLER FOR 70.c */
void usr1_handler(int sig){
    
    memset(buf, 0, BUFSIZE);
    sprintf(buf,"from usr1handler: %s", mystr);
    write(logfd, buf, strlen(buf));
    write(logfd, "\n", 1);
}

int main(int argc, char *argv[]){
    int ret;
    sigset_t mask1, mask2;
    int c_exit;
    int fib_n;
    pid_t cpid;
    pid_t parent = getpid();

    /* PARSE COMMAND LINE */
    if (argc > 1){
        c_exit = atoi(argv[1]);
        fib_n = atoi(argv[2]);
        strcpy(mystr, argv[3]);
    }

    /* BLOCK ALL SIGNALS BUT SIGINT FOR 70.c*/
    sigfillset(&mask1);
    sigdelset(&mask1, SIGINT);
    sigprocmask(SIG_BLOCK, &mask1, NULL);

    /* OPEN LOG FILE */
    logfd = open("log", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (logfd < 0){
        perror("open log");
    }

    /* SET UP HANDLER FOR SIGUSR1 */
    struct sigaction sa; 
    sa.sa_handler = usr1_handler;
    sa.sa_flags = 0;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1){
        perror("sigaction: ");
        exit(1);
    }
    
    /* SET UP MASK TO ALLOW SIGCHLD IN */
    sigfillset(&mask2);
    sigdelset(&mask2, SIGUSR1);

    cpid = fork();
    
    if (cpid < 0){
        perror("fork: ");
        exit(0);
    }

    /* CHILD */
    if (cpid == 0){
        fib(fib_n);

        kill(parent, SIGUSR1);
        kill(parent, SIGUSR2);

        exit(c_exit);
    }

    /* PARENT */
    else{
        sigsuspend(&mask2);

        /* BLOCK ALL SIGNALS BUT SIGCHLD */
        sigemptyset(&mask2);
        sigaddset(&mask2, SIGCHLD);
        sigprocmask(SIG_UNBLOCK, &mask2, NULL);

        wait(&status);

        if(WIFEXITED(status)){
            memset(buf, 0, BUFSIZE);
            sprintf(buf, "child exited with code: %d\n", WEXITSTATUS(status));
            write(logfd, buf, strlen(buf));
        }  
        
        close(logfd);
        exit(0);
    }
}
