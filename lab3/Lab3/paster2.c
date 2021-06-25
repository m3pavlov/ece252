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
#include <pthread.h>
#include "cURL.h"

#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"

int worker(int n);
void producer(RECV_BUF *p_shm_recv_buf, int buf_size);
void consumer(RECV_BUF *p_shm_recv_buf, int buf_size);

/**
 * @brief sleeps (n+1)*1000 milliseconds
 */
int worker(int n)
{
    usleep((n+1)*1000);
    printf("Worker ID=%d, pid = %d, ppid = %d.\n", n, getpid(), getppid());

    return 0;
}


pthread_mutex_t mutex;
int pindex = 0;
int cindex = 0;
int shmid;
int shmid_counter;


RECV_BUF *p_shm_recv_buf;
int *counter;
sem_t *spaces;
sem_t *items;


int main( int argc, char** argv )
{
    // int buffer_size = atoi(argv[1]);
    // int n_consumers = atoi(argv[2]);
    // int n_producers = atoi(argv[3]);
    // int ms_consumer_sleeps = atoi(argv[4]);
    // int n_image = atoi(argv[5]);
    // printf("%u, %u, %u, %u, %u \n", buffer_size, n_consumers, n_producers, ms_consumer_sleeps, n_image);
    int shmid_spaces = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_items = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    spaces = shmat(shmid_spaces, NULL, 0);
    items = shmat(shmid_items, NULL, 0);

    // p_shm_recv_buf = malloc(50*(sizeof(RECV_BUF)));
    // counter = malloc(1*(sizeof(int)));

    shmid_counter = shmget( IPC_PRIVATE, 32, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR );
    counter = shmat( shmid_counter, NULL, 0 );

    // if ( shmid == -1 ) {
    //     perror("shmget");
    //     abort();
    // }
    // *counter = 0;

    int n_consumer = 6;
    int n_producer = 3;
    int buf_size = 5;
    sem_init( spaces, 1, buf_size );
    sem_init( items, 1, 0 );
    pthread_mutex_init( &mutex, NULL );
    int num_child = n_consumer + n_producer;
    int shm_size = sizeof_shm_recv_buf(BUF_SIZE);

    printf("shm_size = %d.\n", shm_size);
    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if ( shmid == -1 ) {
        perror("shmget");
        abort();
    }
    p_shm_recv_buf = shmat(shmid, NULL, 0);
    // p_shm_recv_buf = malloc(buf_size*(10240));

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
    
    curl_global_init(CURL_GLOBAL_DEFAULT);

    for ( i = 0; i < num_child; i++) {
        
        pid = fork();

        if ( pid < 0 ) {        /* fork failed */
            perror("fork");
            abort();
        } else if ( pid == 0 ) { /* child proc */

            if (i < n_producer)
                producer(p_shm_recv_buf, buf_size);
            else
                consumer(p_shm_recv_buf, buf_size);

            break;
        }
    }

    if (pid > 0) {      /* parent proc */

        while (wait(NULL) != -1);
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
    shmdt(p_shm_recv_buf);
    shmctl(shmid, IPC_RMID, NULL);
    sem_destroy(spaces);
    sem_destroy(items);
    pthread_mutex_destroy(&mutex);
    curl_global_cleanup();
    return 0;
}


void producer(RECV_BUF *p_shm_recv_buf, int buf_size) {

    while(counter!=50){
    printf("in producer\n");
    sem_wait(spaces);
    printf("producer can go\n");
    pthread_mutex_lock(&mutex);
    /* write to shared memory here */
    if (p_shm_recv_buf[pindex].size == 0) {

        int server = *counter % 3;

        if (server == 0){
            server = 3;
        }

        shm_recv_buf_init(&p_shm_recv_buf[*counter], 10240);
        get_cURL( *counter, server, &p_shm_recv_buf[pindex] );
        printf("size at pindex: %u = %u\n", pindex, p_shm_recv_buf[pindex].size);
        printf("seq at pindex: %u = %u\n", pindex, p_shm_recv_buf[pindex].seq);
        *counter+=1;
    }
    printf("counter mutex: %u\n",*counter);

    pindex = (pindex+1)%buf_size;
    pthread_mutex_unlock(&mutex);

    sem_post(items);

    printf("after posted items\n");
    }
}


void consumer(RECV_BUF *p_shm_recv_buf, int buf_size) {

    while(counter!= 50){
    sem_wait(items);
    printf("consumer can go\n");
    pthread_mutex_lock(&mutex);
    /* read from shared memory here */
    printf("size at cindex: %u = %u\n", cindex, p_shm_recv_buf[cindex].size);
    printf("seq at cindex: %u = %u\n", cindex, p_shm_recv_buf[cindex].seq);
    if (p_shm_recv_buf[cindex].size != 0) {
        /* SLEEP FOR X TIME */
        RECV_BUF temp = p_shm_recv_buf[cindex];
        p_shm_recv_buf[cindex].size = 0;
        printf("tempSEQ: %u \n", temp.seq);
    }
    printf("counter mutex: %u \n",*counter);
    cindex = (cindex+1)%buf_size;
    pthread_mutex_unlock(&mutex);
    sem_post(spaces);
    }


}