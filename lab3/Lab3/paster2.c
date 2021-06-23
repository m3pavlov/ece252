/* The code is 
 * Copyright(c) 2018-2019 Yiqing Huang, <yqhuang@uwaterloo.ca>.
 *
 * This software may be freely redistributed under the terms of the X11 License.
 */

/**
 * @file   forkN.c
 * @brief  fork N child processes and time the overall execution time  
 */

/******************************************************************************
 * INCLUDE HEADER FILES
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <semaphore.h>

#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"

int worker(int n);
void producer();

typedef struct recv_buf_flat {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

/**
 * @brief sleeps (n+1)*1000 milliseconds
 */
int worker(int n)
{
    usleep((n+1)*1000);
    printf("Worker ID=%d, pid = %d, ppid = %d.\n", n, getpid(), getppid());

    return 0;
}

sem_t sem;
int shmid;


int main( int argc, char** argv )
{

    int n_consumer = 1;
    int n_producer = 1;
    int buf_size = 10240;
    sem_init( &sem, 0, 1);

    int num_child = n_consumer + n_producer;

    int i=0;
    pid_t pid=0;
    pid_t cpids[num_child];
    int state;
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    char url[256];
    RECV_BUF *p_shm_recv_buf;
    int shm_size = sizeof_shm_recv_buf(buf_size);
    char fname[256];
    pid_t pid =getpid();
    pid_t cpid = 0;
    
    printf("shm_size = %d.\n", shm_size);
    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( shmid == -1 ) {
        perror("shmget");
        abort();
    }

    p_shm_recv_buf = shmat(shmid, NULL, 0);
    shm_recv_buf_init(p_shm_recv_buf, buf_size);


    if (argc == 1) {
        strcpy(url, IMG_URL); 
    } else {
        strcpy(url, argv[1]);
    }
    printf("%s: URL is %s\n", argv[0], url);

    curl_global_init(CURL_GLOBAL_DEFAULT);


    for ( i = 0; i < num_child; i++) {
        
        pid = fork();

        if ( pid < 0 ) {        /* fork failed */
            perror("fork");
            abort();
        } else if ( pid == 0 ) { /* child proc */

            if (i < n_producer)
                producer(i);
            else
                consumer(i);

            break;
        } else {                /* parent proc */
            wait(NULL);
            cpids[i] = pid;
            for ( i = 0; i < num_child; i++ ) {
                waitpid(cpids[i], &state, 0);
                if (WIFEXITED(state)) {
                    printf("Child cpid[%d]=%d terminated with state: %d.\n", i, cpids[i], state);
                } 
            }
            if (gettimeofday(&tv, NULL) != 0) {
                perror("gettimeofday");
                abort();
            }
            times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
            printf("Parent pid = %d: total execution time is %.6lf seconds\n", getpid(),  times[1] - times[0]);

        }
        
    }


    sem_destroy(&sem);
    curl_global_cleanup();
    return 0;
}


void producer() {

}