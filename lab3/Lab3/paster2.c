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
#include "lab_png.h"  /* simple PNG data structures  */
#include "crc.h"  /* simple PNG data structures  */
#include "zutil.h"

#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"
#define BUF_SIZE 10240

int worker(int n);
void producer(RECV_BUF *p_shm_recv_buf, int buf_size);
void consumer(RECV_BUF *p_shm_recv_buf, int buf_size);
void idat_initialize(RECV_BUF temp);
void write_chunk_to_file(struct chunk *curr_chunk);
void catpng ();

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


RECV_BUF *p_shm_recv_buf;
int *counter;
U64 *length_all;
sem_t *spaces;
sem_t *items;

// char *inflated_IDAT_data;
U8 *idat_data; //[height_all*(width*4+1)]

int main( int argc, char** argv )
{
    // int buffer_size = atoi(argv[1]);
    // int n_consumers = atoi(argv[2]);
    // int n_producers = atoi(argv[3]);
    // int ms_consumer_sleeps = atoi(argv[4]);
    // int n_image = atoi(argv[5]);
    // printf("%u, %u, %u, %u, %u \n", buffer_size, n_consumers, n_producers, ms_consumer_sleeps, n_image);

    int n_consumer = 1;
    int n_producer = 1;
    int buf_size = 3;
    int num_child = n_consumer + n_producer;
    int shm_size = sizeof_shm_recv_buf(BUF_SIZE);
    int height_all = 6*50;
    int width_all = 400;

    int shmid_buf = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_spaces = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_items = shmget(IPC_PRIVATE, sizeof(sem_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_counter = shmget( IPC_PRIVATE, 32, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR );
    int shmid_length_all = shmget( IPC_PRIVATE, sizeof(U64), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR );
    int shmid_idat_data = shmget(IPC_PRIVATE, height_all*(width_all*4+1), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if ( shmid_buf == -1 || shmid_spaces == -1 || shmid_items == -1 || shmid_counter == -1 || shmid_length_all == -1 || shmid_idat_data == -1) {
        perror("shmget");
        abort();
    }

    p_shm_recv_buf = (RECV_BUF *)shmat(shmid_buf, NULL, 0);

    spaces = shmat(shmid_spaces, NULL, 0);
    items = shmat(shmid_items, NULL, 0);
    counter = shmat(shmid_counter, NULL, 0);
    length_all = shmat(shmid_length_all, NULL, 0);
    idat_data = shmat(shmid_idat_data, NULL, 0);

    sem_init( spaces, 1, buf_size );
    sem_init( items, 1, 0 );
    pthread_mutex_init( &mutex, NULL );

    for (int j = 0; j < buf_size; j++) {
        shm_recv_buf_init(&p_shm_recv_buf[j], shm_size);
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    *counter = 0;
    *length_all = 0;
    int i = 0;
    pid_t pid = 0;
    pid_t cpids[num_child];
    int state;
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    for (i = 0; i < num_child; i++) {

        pid = fork();

        if ( pid < 0 ) {        /* fork failed */
            perror("fork");
            abort();
        } else if ( pid == 0 ) { /* child proc */

            if (i < n_producer) {
                producer(p_shm_recv_buf, buf_size);
            }
            else {
                consumer(p_shm_recv_buf, buf_size);
            }
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
        catpng();

        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
        printf("Parent pid = %d: total execution time is %.6lf seconds\n", getpid(),  times[1] - times[0]);
    }

    /* cleanup */
    shmctl(shmid_buf, IPC_RMID, NULL);
    shmdt(p_shm_recv_buf);

    shmctl(shmid_spaces, IPC_RMID, NULL);
    sem_destroy(spaces);

    shmctl(shmid_items, IPC_RMID, NULL);
    sem_destroy(items);

    shmctl(shmid_counter, IPC_RMID, NULL);
    shmdt(counter);

    shmctl(shmid_length_all, IPC_RMID, NULL);
    shmdt(length_all);

    shmctl(shmid_idat_data, IPC_RMID, NULL);
    shmdt(idat_data);

    pthread_mutex_destroy(&mutex);

    curl_global_cleanup();
    return 0;
}


void producer(RECV_BUF *p_shm_recv_buf, int buf_size) {

    while(*counter < 50){
        sem_wait(spaces);
        pthread_mutex_lock(&mutex);
        /* write to shared memory here */
        {
            if (p_shm_recv_buf[pindex].size == 0) {
                int server = *counter % 3;
                if (server == 0){
                    server = 3;
                }

                printf("producer can go\n");

                printf("checking SIZE: %i at index: %i\n", p_shm_recv_buf[pindex].size, pindex);

                // for (int j = 0; j < buf_size; j++) {
                //     printf("getting SIZE: %i\n", p_shm_recv_buf[j].size);
                // }

                get_cURL( *counter, server, &p_shm_recv_buf[pindex], pindex );

                
                // for (int j = 0; j < buf_size; j++) {
                //     printf("getting SIZE: %i\n", p_shm_recv_buf[j].size);
                // }

                printf("size at pindex: %u = %u\n", pindex, p_shm_recv_buf[pindex].size);
                printf("seq at pindex: %u = %u\n", pindex, p_shm_recv_buf[pindex].buf[33]);
                *counter+=1;

                printf("counter: %u\n",*counter);
            }

            pindex = (pindex+1)%buf_size;
        }
        pthread_mutex_unlock(&mutex);
        sem_post(items);
    }
}

void idat_initialize(RECV_BUF temp){

    
    /* get idat data uncompressed and store in shared mem */

    struct chunk * idat;

    U32 height = 6;
    U32 width = 400;
    U64 uncompressed_size = height*(width*4+1);
    U8 temp_data[uncompressed_size];

    /* using our helper function to get chunk information */
    idat = retrieve_chunk(&temp.buf[33]);

    *length_all += idat->length;
    printf("idat length: %u\n", idat->length);
    printf("total length: %u\n", *length_all);
    mem_inf(&temp_data[0], &uncompressed_size, idat->p_data, idat->length);

    int array_position = temp.seq*uncompressed_size;

    /* uncompressed data goes into IDAT_data array */
    memcpy(&idat_data[array_position], &temp_data[0], uncompressed_size);

    free(idat);
}

void consumer(RECV_BUF *p_shm_recv_buf, int buf_size) {

    while(*counter < 50){
        sem_wait(items);
        // printf("consumer can go\n");
        pthread_mutex_lock(&mutex);
        /* read from shared memory here */
        if (p_shm_recv_buf[cindex].size != 0) {
            /* SLEEP FOR X TIME */

            printf("size at cindex: %u = %u\n", cindex, p_shm_recv_buf[cindex].size);
            printf("seq at cindex: %u = %u\n", cindex, p_shm_recv_buf[cindex].buf[33]);

            /* get data from producer */
            RECV_BUF temp = p_shm_recv_buf[cindex];

            printf("tempSEQ: %u \n", temp.seq);

            unsigned char len[4] = {0};
            memcpy(len, &temp.buf[33], 4);

            printf("tempbufSIZE: %u%u%u%un", len[0], len[1], len[2], len[3]);
            // printf("tempSEQ: %u \n", sizeof(temp.buf));

            idat_initialize(temp);
            int shm_size = sizeof_shm_recv_buf(BUF_SIZE);
            shm_recv_buf_init(&p_shm_recv_buf[cindex], shm_size);

            printf("counter mutex: %u \n", *counter);
        }
        cindex = (cindex+1)%buf_size;
        pthread_mutex_unlock(&mutex);
        sem_post(spaces);
    }
}

void catpng () {

    printf("hello");
    
    U32 height_all = 0;
    U32 width = 0;

    struct chunk *ihdr_all = malloc(sizeof(struct chunk));
    struct chunk *iend_all = malloc(sizeof(struct chunk));
    struct chunk *idat_all = malloc(sizeof(struct chunk));

    /* total (all.png) height and width (width is same for all PNGs) */
    height_all = 300;
    int height = 6;
    width = 400;

    // /* variables that only need to be set once, doesn't matter which inputted PNG it uses */
    ihdr_all->length = 13;
    ihdr_all->type[0] = "I";
    ihdr_all->type[1] = "H";
    ihdr_all->type[2] = "D";
    ihdr_all->type[3] = "R";


    iend_all->length = 0;
    iend_all->type[0] = "I";
    iend_all->type[1] = "E";
    iend_all->type[2] = "N";
    iend_all->type[3] = "D";


    /* initialize array that stores all IDAT data values (compressed) */
    U8 IDAT_data_all[*length_all]; /* length of all.png IDAT chunk data (compressed) */

    U64 array_position = 50*height*(width*4+1);

    printf("%u, %u \n", length_all, array_position);

    /* compress data */
    mem_def(&IDAT_data_all[0], length_all, idat_data, array_position, -1); /* compressing data of all PNG before it goes in IDAT chunk */

    // /* initializes length of complete idat and data */
    // idat_all->length = *length_all;
    // idat_all->p_data = &IDAT_data_all[0];

    // /* initializes type values in a character array (for crc use and to initialize type for IDAT chunk) */
    // U8 idat_type[4] = {73,68,65,84};
    // U8 ihdr_type[4] = {73,72,68,82};

    // memcpy(&idat_all->type[0], &idat_type[0], 4);

    // /* initializes arrays for crc check */
    // unsigned char ihdr_buf[DATA_IHDR_SIZE+CHUNK_TYPE_SIZE];
    // unsigned char idat_buf[*length_all+CHUNK_TYPE_SIZE];

    // /* copy a combination of data and type to new character array to check crc values */
    // memcpy(&idat_buf[0], &idat_type[0], CHUNK_TYPE_SIZE);
    // memcpy(&idat_buf[4], &IDAT_data_all[0], length_all);

    // printf("hello");

    // /* initialize the crc values for the new idat */
    // U32 crc_idat = crc(&idat_buf[0], *length_all+CHUNK_TYPE_SIZE);

    // /* update length and crc values in chunks with reversed values */
    // idat_all->crc = ntohl(crc_idat);

    // U32 iend_crc_temp = ntohl(iend_all->crc);
    // iend_all->crc = iend_crc_temp;

    // /* create array to store values for IHDR data */
    // U8 ihdr_data_all[DATA_IHDR_SIZE];

    // height_all = ntohl(height_all);
    // width = ntohl(width);

    // for (int i = 0; i < 4; i++){
    //     /* calculates height and width and updates IHDR chunk data */
    //     ihdr_data_all[3-i] = width/((int)pow(256, 3 - i));
    //     ihdr_data_all[7-i] = height_all/((int)pow(256, 3 - i));
    //     height_all = height_all % ((int)pow(256, 3 - i));
    //     width = width % ((int)pow(256, 3 - i));
    // }
    // /* rest of IHDR data (fixed values) */
    // ihdr_data_all[8] = 8;
    // ihdr_data_all[9] = 6;
    // ihdr_data_all[10] = 0;
    // ihdr_data_all[11] = 0;
    // ihdr_data_all[12] = 0;

    // free(ihdr_all->p_data);
    // ihdr_all->p_data = &ihdr_data_all[0]; /* updating IHDR data (new height and width) */

    // /* update crc value in IHDR chunk with reversed values */
    // memcpy(&ihdr_buf[0], &ihdr_type[0], 4);
    // memcpy(&ihdr_buf[4], &ihdr_data_all[0], 13);
    // U32 crc_ihdr = crc(ihdr_buf, DATA_IHDR_SIZE+CHUNK_TYPE_SIZE);
    // ihdr_all->crc = ntohl(crc_ihdr);

    // /* write 8 byte png header */
    // FILE *all_png;
    // all_png = fopen("all.png", "wb");
    // U8 eight_byte_hdr[PNG_SIG_SIZE] = {137, 80, 78, 71, 13, 10, 26, 10};
    // fwrite(&eight_byte_hdr, sizeof(U8), PNG_SIG_SIZE, all_png);
    // fclose(all_png);

    // /* writes each chunk to our file */
    // write_chunk_to_file(ihdr_all);
    // write_chunk_to_file(idat_all);
    // write_chunk_to_file(iend_all);

    // /* frees chunks */
    // free (iend_all->p_data);
    // free (ihdr_all);
    // free (idat_all);
    // free (iend_all);
}


void write_chunk_to_file(struct chunk *curr_chunk) {
    FILE *fp;
    fp = fopen("all.png", "ab");

    U32 length = (ntohl(curr_chunk->length));
    U32 crc = (curr_chunk->crc);

    /* writes each chunk attribute to file */
    fwrite(&length, sizeof(U32), 1, fp);
    fwrite(&curr_chunk->type, sizeof(U8), 4, fp);
    fwrite(&curr_chunk->p_data[0], sizeof(U8), curr_chunk->length, fp);
    fwrite(&crc, sizeof(U32), 1, fp);

    fclose(fp);
}

