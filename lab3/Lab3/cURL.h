#include <stdlib.h>

#define IMG_URL_1 "http://ece252-1.uwaterloo.ca:2530/image?img="
#define IMG_URL_2 "http://ece252-2.uwaterloo.ca:2530/image?img="
#define IMG_URL_3 "http://ece252-3.uwaterloo.ca:2530/image?img="
#define ECE252_HEADER "X-Ece252-Fragment: "
#define DUM_URL "https://example.com/"
#define BUF_SIZE 10240 /* 1024*10 = 10K */

/* This is a flattened structure, buf points to 
   the memory address immediately after 
   the last member field (i.e. seq) in the structure.
   Here is the memory layout. 
   Note that the memory is a chunk of continuous bytes.

   On a 64-bit machine, the memory layout is as follows:

   +================+
   | buf            | 8 bytes
   +----------------+
   | size           | 8 bytes
   +----------------+
   | max_size       | 8 bytes
   +----------------+
   | seq            | 4 bytes
   +----------------+
   | padding        | 4 bytes
   +----------------+
   | buf[0]         | 1 byte
   +----------------+
   | buf[1]         | 1 byte
   +----------------+
   | ...            | 1 byte
   +----------------+
   | buf[max_size-1]| 1 byte
   +================+
*/
typedef struct recv_buf_flat {
    char *buf; /* hold a copy of received data */
    size_t size;        /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes);
int sizeof_shm_recv_buf(size_t nbytes);
int get_cURL( int image_option, int server, RECV_BUF *p_shm_recv_buf, int pindex );
