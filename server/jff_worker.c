#include "jff_config.h"
#include "jff_ring.h"
#include "jff_worker.h"
#include "jff_sock.h"

extern jff_ring_t ring;
extern struct server_s server;
volatile int    total_line_num;

static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;

#define ADD_TO_LIST(list, elem)   \
    if (list) {                   \
        elem->next = list->next;  \
    }                             \
    list = c;                     \

static void * file_worker_run(void *);
static void * net_worker_run(void *);

static int process_events(thread_t *t);
static int process_accept_event(thread_t *t);
static int process_read_event(connection_t *c);
static int process_write_event(connection_t *c);
static int process_request(connection_t *c);
static int notify_client_exit(thread_t *t);
static connection_t* connection_new(thread_t *t);
static int set_thread_affinity(int i);


int workers_init(server_t *s)
{
    int         i, ep;
    thread_t   *t;
    struct epoll_event *ee;

    if ((thread_pool = calloc(s->ncpus, sizeof(thread_t))) == NULL) {
        return -1;
    }

    nthreads = s->ncpus;

    for (i = 0; i < nthreads; i++) {
        t = &thread_pool[i];

        t->state = STATE_FREE;
        t->listen_fd = s->listen_fd;
        t->max_connections = s->max_connection;

        if ((ep = epoll_create(t->max_connections)) < 0) {
            jff_log(JFF_ERROR, "epoll_create error!");
            return -1;
        }

        if ((ee = (struct epoll_event *) calloc(t->max_connections,
                                                sizeof(struct epoll_event))) == NULL)
        {
            jff_log(JFF_ERROR, "memory low when allocating epoll_events");
            return -1;
        }

        t->epoll_fd = ep;
        t->epoll_events = ee;
        t->accept_held = 0;
    }

    return 0;
}

int workers_fun()
{
    int i;
    thread_t *t;


    for (i = 0; i < 1; i++) {
        t = &thread_pool[i];
        t->cpuid = 0;
        pthread_create(&t->tid, NULL, file_worker_run, t);
        thread_pool[i].type = WOKER_FILE;
    }

    for (i = 1; i < server.ncpus; i++) {
        t = &thread_pool[i];
        t->cpuid = i;
        pthread_create(&(thread_pool[i].tid), NULL, net_worker_run, t);
        thread_pool[i].type = WOKER_NET;
    }

    return 0;
}

int workers_off_duty()
{
    int i;
    thread_t *t;

    for (i = 0; i < nthreads; i++) {
        t = &thread_pool[i];

        if (t && t->tid && pthread_join(t->tid, NULL) != 0) {
            fprintf(stderr, "pthread_join error!\n");
            return -1;
        }
    }

    return 0;
}

int workers_destroy()
{

    return 0;
}

static void * file_worker_run(void *arg)
{
    FILE         *fp;
    element_t *e;
    thread_t   *t;
    int         offset = 0;
    int    len;

    t = (thread_t *) arg;

    set_thread_affinity(t->cpuid);

    if (set_rlimit() == -1) {
        return NULL;
    }

    if ((fp = fopen(server.data_file_name, "rb")) == NULL) {
        return 0;
    }

    while(1) {

        if ((e = calloc(1, sizeof(element_t))) == NULL) {
            jff_log(JFF_ERROR, "memory low when alloc ring_element");
            break;
        }

        if (fgets(e->data, 256, fp) == NULL) {
            break;
        }

        len = strlen(e->data);
        e->length = len;
        e->offset = offset;
        while (jff_ring_enqueue(&ring, e) == ERROR) {

        }

        total_line_num++;

        offset += len - (len - 2) / 3;
    }

    jff_log(JFF_INFO, "load finish");

    return 0;
}


static void * net_worker_run(void *args)
{
    thread_t     *t;
    connection_t *c, *cc;


    t = (thread_t *) args;

    // do some preparation
    set_thread_affinity(t->cpuid);

    if (set_rlimit() == -1) {
        return NULL;
    }

    for (;;) {
        if (t->state == STATE_EXIT) {
            notify_client_exit(t);
        }

        process_events(t);

        /* process postponed events */
        for (c = t->posted_connection_list; c;) {
            cc = c;
            c = c->next;

            if (cc->read) {
                process_read_event(cc);
            } else {
                process_write_event(cc);
            }
        }

        t->posted_connection_list = NULL;
    }

    return 0;
}

static int process_events(thread_t *t)
{
    struct epoll_event ev;
    int                nevents;
    int                timer;
    int                err, i;
    int                fd;
    connection_t      *c;


    timer = 500;

    if (t->cur_connections < t->max_connections * 3 / 4) {
        if (pthread_mutex_trylock(&accept_mutex) == 0) {

            //jff_log(JFF_INFO, "cought accept_mutex");
            if (t->accept_held == 0) {
                ev.events = EPOLLIN;
                ev.data.fd = t->listen_fd;
                if (epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, t->listen_fd, &ev) == -1) {
                    fprintf(stderr, "epoll_ctl error:%s", strerror(errno));
                    return -1;
                }

                //jff_log(JFF_INFO, "add listen fd to epoll");
                t->accept_held = 1;
            }

        } else {
            if (t->accept_held == 1) {
                ev.events = EPOLLIN;
                if (epoll_ctl(t->epoll_fd, EPOLL_CTL_DEL, t->listen_fd, &ev) == -1) {
                    fprintf(stderr, "epoll_ctl error:%s", strerror(errno));
                    return -1;
                }
            }

            t->accept_held = 0;
        }
    }

    nevents = epoll_wait(t->epoll_fd, t->epoll_events, t->max_connections, timer);

    err = (nevents == -1) ? errno : 0;

    if (err) {
        if (err == EINTR) {
            jff_log(JFF_ERROR, "epoll_wait() was interrupted");
            return OK;
        }
    }

    if (nevents == 0) {
        goto leave;
    }

    for (i = 0; i < nevents; i++) {
        fd = t->epoll_events[i].data.fd;

        if (fd == t->listen_fd) {
            //jff_log(JFF_INFO, "accept one");
            process_accept_event(t);
            continue;
        }

        if (t->epoll_events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
            c = t->epoll_events[i].data.ptr;

            ev.events = EPOLLIN | EPOLLET;
            epoll_ctl(t->epoll_fd, EPOLL_CTL_DEL, c->fd, &ev);

            /* add to postponed list */
            c->next = t->posted_connection_list;
            t->posted_connection_list = c;
        }

        if (t->epoll_events[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
            c = t->epoll_events[i].data.ptr;

            ev.events = EPOLLOUT | EPOLLET;
            epoll_ctl(t->epoll_fd, EPOLL_CTL_DEL, c->fd, &ev);

            /* add to postponed list */
            c->next = t->posted_connection_list;
            t->posted_connection_list = c;
        }
    }

 leave:
    if (t->accept_held == 1) {
        //jff_log(JFF_INFO, "release accept_mutex");
        pthread_mutex_unlock(&accept_mutex);
    }

    return OK;
}

static int process_accept_event(thread_t *t)
{
    struct sockaddr_in cliaddr;
    socklen_t          clilen;
    int                clifd;
    connection_t      *c;
    struct epoll_event ev;


    if ((clifd = accept(t->listen_fd, (struct sockaddr*) &cliaddr, &clilen)) < 0) {
        jff_log(JFF_ERROR, "accept error:%s", strerror(errno));
        return ERROR;
    }

    if ((c = connection_new(t)) == NULL) {
        jff_log(JFF_ERROR, "memory low");
        return ERROR;
    }

    //jff_log(JFF_INFO, "accept a new connection");

    tcp_set_nonblock(clifd);
    c->fd = clifd;
    c->read = 1;
    c->peer_addr = cliaddr;
    c->socklen = clilen;
    c->recv_buf_size = 1;
    c->recv_buf_pos = c->send_buf_pos = 0;

    ev.data.ptr = c;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, c->fd, &ev) < 0) {
        if (errno == EEXIST) {
            jff_log(JFF_WARNING, "fd already exist");
            return ERROR;
        }
    }

    return OK;
}


static int process_read_event(connection_t *c)
{
    int ret;
    struct epoll_event ev;
    thread_t  *t;

    t = c->thread;

    ret = read(c->fd, c->recv_buf, 1);

    if (ret > 0) {
        //c->recv_buf_pos += ret;

        //if (c->recv_buf_pos == c->recv_buf_size) {
        process_request(c);
        //}
    } else if (ret == 0) {
        /* peer down */
        goto error;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ev.events = EPOLLIN | EPOLLET;
            ev.data.ptr = c;
            epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, c->fd, &ev);
        } else {
            jff_log(JFF_ERROR, "socket read from %d error:%s",strerror(errno));
            goto error;
        }
    }

    return 0;

 error:
    close(c->fd);
    ADD_TO_LIST(t->free_connection_list, c);
    return 0;
}

static int process_write_event(connection_t *c)
{
    int ret, err;
    struct epoll_event ev;
    thread_t  *t;




    t = c->thread;

    ret = write(c->fd, c->send_buf, c->send_buf_size - c->send_buf_pos);

    if (ret < 0) {
        err = errno;

        if (err == EAGAIN || err == EWOULDBLOCK) {
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = c;
            epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, c->fd, &ev);
        }

        if (err == EPIPE) {
            /* peer down */
            close(c->fd);
            ADD_TO_LIST(t->free_connection_list, c)
        }
        //TODO: other errors
    } else {
        c->send_buf_pos += ret;

        if (c->send_buf_pos == c->send_buf_size) {
            /* keep recving from client */
            //jff_log(JFF_INFO, "write all");

            c->read = 1;
            c->recv_buf_pos = c->recv_buf_size = 0;
            ev.data.ptr = c;
            ev.events = EPOLLIN | EPOLLET;
            epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, c->fd, &ev);
        } else {
            ev.data.ptr = c;
            ev.events = EPOLLOUT | EPOLLET;
            epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, c->fd, &ev);
        }
    }

    return 0;
}


static int process_request(connection_t *c)
{
    element_t     *e;
    char     *p, *p1, *p2, *q1, *q2;
    thread_t *t;
    int       len, linum, n;
    struct epoll_event ev;


    t = c->thread;

    c->send_buf_size = c->send_buf_pos = 0;

    if ((linum = jff_ring_dequeue(&ring, (void **)&e)) == ERROR) {
        /* the queue is empty */
        t->state = STATE_EXIT;

        sprintf(c->send_buf, "%d ", total_line_num);

        c->send_buf[188] = 'C';
        c->send_buf[189] = 'O';
        c->send_buf[190] = 'B';
        c->send_buf[191] = 'B';

        goto send;
    }

    len = e->length - 2;;

    p1 = e->data;
    p2 = e->data + len / 3 - 1;
    q1 = p2 + len / 3 + 1;
    q2 = e->data + len - 1;

    /* linum length stuff */
    p = c->send_buf;
    n = sprintf(p, "%d", linum);
    p += n;
    p += sprintf(p, " %d %d ", len - len / 3 + n + 2, e->offset);

    /* data start from 40 */
    p = c->send_buf + 40;
    p += sprintf(p, "%d", linum);
    while(q2 >= q1) { *p++ = *q2; q2--; }

    while(p2 >= p1) { *p++ = *p2; p2--; }

    *p++ = '\r';
    *p++ = '\n';

 send:
    c->read = 0;
    c->send_buf_size = 192;

    ev.events = EPOLLOUT | EPOLLET;
    ev.data.ptr = c;
    if (epoll_ctl(t->epoll_fd, EPOLL_CTL_ADD, c->fd, &ev) == -1) {
        jff_log(JFF_ERROR, "call epoll_ctl error:%s", strerror(errno));
        return -1;
    }

    return 0;
}


static int notify_client_exit(thread_t *t)
{

    return 0;
}


static connection_t* connection_new(thread_t *t)
{
    connection_t *c;

    if (t->free_connection_list) {
        c = t->free_connection_list;
        t->free_connection_list = c->next;
        return c;
    }

    if ((c = (connection_t *) calloc(1, sizeof(connection_t))) == NULL) {
        return NULL;
    }
    c->thread = t;
    c->next = NULL;

    return c;
}

static int set_thread_affinity(int i)
{
    cpu_set_t  mask;

    CPU_ZERO(&mask);
    CPU_SET(i, &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
        jff_log(JFF_WARNING, "set thread affinity failed");
        return 0;
    }

    return 0;
}
