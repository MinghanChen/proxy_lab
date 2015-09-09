#include <stdio.h>
#include <string.h>
#include "csapp.h"
#include "sbuf.h"


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 4
#define SBUFSIZE 16

void * deal_req(int connfd);

void *thread(void *vargp);

/*sbuf_t is a buffer using producer-consumer model. When there is a connection request
from client, the sbuf_t will assign it to a free thread in the thread pool*/
sbuf_t sbuf;
/*the main function take the input as the port number. Then it create four threads in
the thread pool. */
int main(int argc, char **argv)
{
    int i, listenfd, connfd;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    sbuf_init(&sbuf, SBUFSIZE);
    listenfd = Open_listenfd(argv[1]);

    for (i = 0; i < NTHREADS; i++) {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    while (1) {
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
    }

    return 0;
}

/*a new created thread will detach itself. Then it will wait to handle the request from
client in a while loop.*/
void *thread(void *vargp) {
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove( &sbuf);
        deal_req(connfd);
        Close(connfd);
    }
}
