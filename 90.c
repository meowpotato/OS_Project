/*Sabrina Smith
 *CMPS 360
 *90.c
 *Producer/Consumer Synchronization on Shared Memory w/ Threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define BUFSIZE 100
#define READSIZE 50
#define DEBUG 0
#define PSEM 1
#define CSEM 2
#define ASEM 3

/*GLOBAL VARIABLES*/
char *shared; 
int LIMIT;
int retval = 0;
char mystr[BUFSIZE];
int logfd;
char buf[BUFSIZE];
char smallbuf[0];
int status = 0;
pid_t cpid;

/*struct sigaction{
    void        (*sa_handler)(int);
    void        (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t    sa_mask;
    int         sa_flags;
    void        (*sa_restorer)(void);
};*/

union{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
} my_semun;

struct sembuf grab[1];
struct sembuf release[1];
struct sembuf grabCSEM[1];
struct sembuf grabPSEM[1];
struct sembuf grabASEM[1];
struct sembuf releaseCSEM[1];
struct sembuf releasePSEM[1];
struct sembuf releaseASEM[1];

int sem_value;
int semid;

int fib(int n){
    if (n == 0) return 1;
    if (n == 1) return 1;
    return fib(n-1) + fib(n-2);
}

/* SIGUSR1 HANDLER FOR 70.c */
void usr1_handler(int sig){   
    memset(buf, 0, BUFSIZE);
    sprintf(buf,"from usr1_handler: %s", mystr);
    write(logfd, buf, strlen(buf));
    write(logfd, "\n", 1);
}

/* SIGINT HANDLER FOR 80.c */
void int_handler(int sig){
    if (cpid == 0){
        return;
    }

    memset(buf, 0, BUFSIZE);
    sprintf(buf, "from int_handler: got it!");
    write(logfd, buf, strlen(buf));
    write(logfd, "\n", 1);
}

void *consumer(void * dummy);
void *producer(void * dummy);

int main(int argc, char *argv[]){
    int ret;
    int dummy;
    sigset_t mask1, mask2;
    int c_exit;
    int fib_n;
    char input[READSIZE];
    pid_t parent = getpid();
    

    /* GET IPC KEY */
    getcwd(buf, BUFSIZE);
    strcat(buf, "/foo");

    key_t ipckey = ftok(buf, 17);

    /* SHARED MEMORY */
    int shmid = shmget(ipckey, READSIZE, IPC_CREAT | 0666);
    LIMIT = (strlen(shared)/2);

    int nsems = 0;
    semid = semget(ipckey, nsems, 0666 | IPC_CREAT);
    if (semid < 0){
        printf("Error - %s\n", strerror(errno));
        _exit(1);
    }

    /* GRAB SETUP */
    grab[0].sem_num = 0;
    grab[0].sem_flg = SEM_UNDO;
    grab[0].sem_op = -1;

    /* RELEASE SETUP */
    grab[0].sem_num = 0;
    grab[0].sem_flg = SEM_UNDO;
    grab[0].sem_op = +1;

    my_semun.val = 0;
    semctl(semid, 0, SETVAL, my_semun);

    /* SET UP SEMAPHORES FOR PRODUCER AMD CONSUMER 90.c */
    my_semun.val = 1;
    if (semctl(semid, PSEM, SETVAL, my_semun) == -1){
        perror("setting mysemun.val:");
    }
    if (DEBUG)
        printf("PSEM val: %d\n", semctl(semid, PSEM, GETVAL, 0));

    my_semun.val = 0;
    if (semctl(semid, CSEM, SETVAL, my_semun) == -1){
        perror("setting mysemun.val:");
    }
    if (DEBUG)
        printf("CSEM val: %d\n", semctl(semid, CSEM, GETVAL, 0));
    
    if (semctl(semid, ASEM, SETVAL, my_semun) == -1){
        perror("setting mysemun.val:");
    }
    if (DEBUG)
        printf("ASEM val: %d\n", semctl(semid, ASEM, GETVAL, 0));

    grabPSEM[0].sem_flg = SEM_UNDO;
    grabPSEM[0].sem_num = PSEM;
    grabPSEM[0].sem_op = -1;

    releaseCSEM[0].sem_flg = SEM_UNDO;
    releaseCSEM[0].sem_num = CSEM;
    releaseCSEM[0].sem_op = +1;
  
    grabCSEM[0].sem_flg = SEM_UNDO;
    grabCSEM[0].sem_num = CSEM;
    grabCSEM[0].sem_op = -1;
    
    releasePSEM[0].sem_flg = SEM_UNDO;
    releasePSEM[0].sem_num = PSEM;
    releasePSEM[0].sem_op = +1;
 
    /* MESSAGE QUEUE */
    struct{
        long type;
        char text[READSIZE];
    } mymsg;

    int mqid = msgget(ipckey, IPC_CREAT | 0666);

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

    /* SET UP HANDLER FOR SIGINT */
    struct sigaction sa2;
    sa2.sa_handler = int_handler;
    sa2.sa_flags = 0;
    sigfillset(&sa2.sa_mask);

    if(sigaction(SIGINT, &sa2, NULL) == -1){
        perror("sigaction ");
        exit(1);
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
    
    /* FORK CHILD */
    cpid = fork();
    
    if (cpid < 0){
        perror("fork: ");
        exit(0);
    }

    /* CHILD */
    if (cpid == 0){
        int received;
    
        /* CALL FIB */
        fib(fib_n);

        /* SEND KILLS TO PARENT */
        kill(parent, SIGUSR1);
        kill(parent, SIGUSR2);
        kill(parent, SIGINT);

        /* GRAB MESSAGE FROM MESSAGE QUEUE AND WRITE TO LOG */
        received = msgrcv(mqid, &mymsg, sizeof(mymsg), 0, 0);
        if (received > 0){
            sprintf(buf, "msg from child: %s\n", mymsg.text);
            write(logfd, buf, strlen(buf));
        } 

        /* GRAB SEMAPHORE */
        semop(semid, grab, 1);

        /* ATTACH TO SHARED MEMORY */
        shared = shmat(shmid, (void*) 0, 0);

        /* WRITE SHARED MEM TO LOG */
        memset(buf, 0, BUFSIZE);
        strcpy(buf, "mem from child: ");
        strcat(buf, shared);
        strcat(buf, "\n");
        write(logfd, buf, strlen(buf));

        /* DETACH FROM SHARED MEMORY */
        shmdt(shared);

        exit(c_exit);
    }

    /* PARENT */
    else{
        sigsuspend(&mask2);

        /* GET FIRST STRING */
        fgets(input, READSIZE, stdin);

        /* SEND INPUT TO MESSAGE QUEUE */
        memset(mymsg.text, 0, READSIZE);
        strcpy(mymsg.text, input);
        mymsg.type = 1;
        msgsnd(mqid, &mymsg, sizeof(mymsg), 0);

        /* ATTACH TO SHARED MEMORY */
        shared = shmat(shmid, (void*) 0, 0);

        /* GET SECOND STRING */        
        fgets(input, READSIZE, stdin);
        
        /* SET INPUT TO SHARED MEMORY */
        strcpy(shared, input);
        
        /* RELEASE SEMAPHORE */
        semop(semid, release, 1);

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
        
        /* CREATE CONSUMER THREAD */
        pthread_t cthr;
        if (pthread_create(&cthr, NULL, consumer, (void *)&dummy) != 0)
            fprintf(stderr, "Error creating thread\n");

        /* CREATE PRODUCER THREAD */
        pthread_t pthr;
        if (pthread_create(&pthr, NULL, producer, (void *)&dummy) != 0)
            fprintf(stderr, "Error creating thread\n");

        /* JOIN THREADS */
        if ((pthread_join(pthr, (void*)&retval)) < 0)
            perror("pthread_join");
        else
            if (DEBUG) write(logfd, "producer joined\n", 15);
        if ((pthread_join(cthr, (void*)&retval)) < 0)
            perror("pthread_join");
        else
            if (DEBUG) write(logfd, "consumer joined\n", 15);

        /* CLOSE LOG FILE */
        close(logfd);

        /* END MESSAGE QUEUE */
        ret = msgctl(mqid, IPC_RMID, NULL);
        if (ret < 0){
            perror("msgctl: ");
        }
    
        /* DETACH FROM SHARED MEMORY */
        shmdt(shared);
        shmctl(shmid, IPC_RMID, 0);

        /* REMOVE SEMAPHORES */
        if ((semctl(semid, 0, IPC_RMID)) < 0){
            perror("semctl IPC_RMID");
            exit(EXIT_FAILURE);
        }

        exit(0);
    }
}

/* CONSUMER THREAD FUNCTION */
void *consumer(void *dummy){
    int count = 0;
    shared += 1;
    while (1){
        /* IF HIT THE LIMIT DON'T CONTINUE */
        if (count == LIMIT)
            break;

        /* GRAB PSEM */
        semop(semid, grabCSEM, 1);

        /* CRITICAL CODE */
        strcpy(smallbuf, shared);
        write(logfd, smallbuf, 0);
        shared += 2;

        count++;
        /* RELEASE PSEM */
        semop(semid, releasePSEM, 1);
    }
    pthread_exit(0);
}

/* PRODUCER THREAD FUNCTION */
void *producer(void *dummy){
    int count = 0;
    while(1){
        /* IF HIT THE LIMIT DON'T CONTINUE */
        if (count == LIMIT)
            break;
        
        /* GRAB PSEM */
        semop(semid, grabPSEM, 1);
        
        /* CRITICAL CODE */
        strcpy(smallbuf, shared);
        write(logfd, smallbuf, 0);
        shared += 2;

        count++;
        /* RELEASE CSEM */
        semop(semid, releaseCSEM, 1);
    }
    pthread_exit(0);
}
