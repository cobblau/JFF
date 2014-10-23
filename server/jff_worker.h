#ifndef _JFF_WORKER_H
#define _JFF_WORKER_H

#include "jff_config.h"

#define  WOKER_FILE  0
#define  WOKER_NET 1

#define  STATE_FREE    0
#define  STATE_WORKING 1
#define  STATE_EXIT    2

typedef struct connection_s connection_t;
typedef struct thread_s     thread_t;

struct connection_s {
    thread_t          *thread;

    int                fd;
    int                read;     /* 1:read 0:write */

    struct sockaddr_in peer_addr;
    socklen_t          socklen;

    char               recv_buf[8];
    unsigned int       recv_buf_size;
    unsigned int       recv_buf_pos;

    char               send_buf[256];
    unsigned int       send_buf_size;
    unsigned int       send_buf_pos;

    struct connection_s *next;
};

struct thread_s {
    pthread_t  tid;
    int        cpuid;
    int        type;
    int        state;

    int        listen_fd;

    int                 epoll_fd;
    struct epoll_event *epoll_events;

    int        max_connections;
    int        cur_connections;

    connection_t *connection_list;
    connection_t *free_connection_list;
    connection_t *posted_connection_list;

    int        accept_held;
};

thread_t   *thread_pool;
int         nthreads;

int workers_init(struct server_s *s);
int workers_fun();
int workers_off_duty();
int workers_destroy();

int file_worker();
int net_worker();

#endif
