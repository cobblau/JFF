#ifndef _JFF_WORKER_H
#define _JFF_WORKER_H

#include "jff_config.h"

#define  STATE_FREE    0
#define  STATE_WORKING 1
#define  STATE_EXIT    2

typedef struct worker_s  worker_t;
typedef struct uthread_s uthread_t;

struct uthread_s {
    int      uthid;

    int                fd;
    int                read;     /* 1:read 0:write */

    char               recv_buf[256];
    unsigned int       recv_buf_size;
    unsigned int       recv_buf_pos;
};

struct worker_s {
    pthread_t  tid;
    int        cpuid;
    int        state;
    int        file_fd;

    int                 epoll_fd;
    struct epoll_event *epoll_events;

    uthread_t *thread_pool;
    int        thread_pool_size;

    int        uth_alive;
};

worker_t   *worker_pool;
int         nworkers;

int workers_init(client_t *c);
int workers_fun();
int workers_off_duty();
int workers_destroy();

int file_worker();
int net_worker();

#endif
