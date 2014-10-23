#include "jff_worker.h"
#include "jff_sock.h"

extern client_t client;
extern element_t buffer[MAX_LINE];
extern int off_map[16];

static void * net_worker_run(void *);
static int    process_response(int fd, char *buf, int len);
static int set_thread_affinity(int i);
static inline int count_real_offset(int offset, int linum);

static int jff_atoi(char *s, int n)
{
    int  value;

    if (n == 0) {
        return ERROR;
    }

    for (value = 0; n--; s++) {
        if (*s < '0' || *s > '9') {
            return ERROR;
        }

        value = value * 10 + (*s - '0');
    }

    return value < 0 ? ERROR : value;
}

int workers_init(client_t *c)
{
    int         i, ep;
    worker_t   *w;
    struct epoll_event *ee;

    if ((worker_pool = calloc(c->ncpus, sizeof(worker_t))) == NULL) {
        return -1;
    }

    nworkers = c->ncpus;

    for (i = 0; i < nworkers; i++) {
        w = &worker_pool[i];

        w->state = STATE_FREE;

        if ((ep = epoll_create(1024)) < 0) {
            jff_log(JFF_ERROR, "epoll_create error!");
            return -1;
        }

        if ((ee = (struct epoll_event *) calloc(c->concurrent_num,
                sizeof(struct epoll_event))) == NULL)
            {
                jff_log(JFF_ERROR, "memory low when allocating epoll_events");
                return -1;
            }

        w->epoll_fd = ep;
        w->epoll_events = ee;

        if ((w->thread_pool = (uthread_t *) calloc(c->concurrent_num, sizeof(uthread_t))) == NULL) {
            jff_log(JFF_ERROR, "memory low when allocating uthread_pool");
            return -1;
        }

        w->thread_pool_size = c->concurrent_num;
        w->uth_alive = 0;
	w->file_fd = c->fd;
    }

    return 0;
}

int workers_fun()
{
    int i;
    worker_t *w;

    /*
    for (i = 0; i < 1; i++) {
        w = &worker_pool[i];
        w->cpuid = i;
        pthread_create(&w->tid, NULL, file_worker_run, w);
    }
    */
    for (i = 0; i < nworkers; i++) {
        w = &worker_pool[i];
        w->cpuid = i;

        if (pthread_create(&w->tid, NULL, net_worker_run, w) != 0) {
            jff_log(JFF_ERROR, "pthread_create error");
            return -1;
        }
    }

    return 0;
}

int workers_off_duty()
{
    int i;
    worker_t *w;

    for (i = 0; i < nworkers; i++) {
        w = &worker_pool[i];

        if (w && w->tid && pthread_join(w->tid, NULL) != 0) {
            jff_log(JFF_ERROR, "pthread_join error!");
            return -1;
        }
    }

    return 0;
}

int workers_destroy()
{
    free(worker_pool);
    return 0;
}

void * net_worker_run(void *args)
{
    worker_t     *w;
    uthread_t    *uth;
    int           i, nevents, timer, err, ret;
    int           n;
    struct epoll_event ev;


    w = (worker_t *) args;

    set_thread_affinity(w->cpuid);

    if (set_rlimit() == -1) {
        return NULL;
    }

    for (i = 0; i < w->thread_pool_size; i++) {
        uth = &w->thread_pool[i];

        if ((uth->fd = tcp_connect(client.server_addr, client.server_port)) == -1) {
            jff_log(JFF_ERROR, "can't connect to server");
            return NULL;
        }

        tcp_set_keepalive(uth->fd);
        tcp_set_nodelay(uth->fd);
        tcp_set_nonblock(uth->fd);

        ev.events = EPOLLOUT;
        ev.data.ptr = uth;
        if (epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, uth->fd, &ev) == -1) {
            fprintf(stderr, "epoll_ctl error:%s", strerror(errno));
            return NULL;
        }

        w->uth_alive++;
    }

    timer = 500;

    for (;;) {

        if (w->uth_alive == 0) {  /* there are no user thread any more */
            break;
        }

        nevents = epoll_wait(w->epoll_fd, w->epoll_events, w->thread_pool_size, timer);

        err = (nevents == -1) ? errno : 0;

        if (err) {
            if (err == EINTR) {
                jff_log(JFF_ERROR, "epoll_wait() was interrupted");
                return OK;
            }
        }

        if (nevents == 0) {
            continue;
        }

        for (i = 0; i < nevents; i++) {
            if (w->epoll_events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
                uth = w->epoll_events[i].data.ptr;

                ev.events = EPOLLIN;
                epoll_ctl(w->epoll_fd, EPOLL_CTL_DEL, uth->fd, &ev);

                n = read(uth->fd, uth->recv_buf + uth->recv_buf_pos, uth->recv_buf_size - uth->recv_buf_pos);

                if (n > 0) {
                    uth->recv_buf_pos += n;

                    if (uth->recv_buf_pos == uth->recv_buf_size) {
                        ret = process_response(w->file_fd, uth->recv_buf, uth->recv_buf_size);
                        if (ret == -2) {
                            /* recv finished */
                            //jff_log(JFF_INFO, "close %d", uth->fd);
                            close(uth->fd);
                            w->uth_alive--;
                        } else {
                            /* recv continue */
                            ev.events = EPOLLOUT;
                            ev.data.ptr = uth;
                            epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, uth->fd, &ev);
                        }
                    } else {
                        ev.events = EPOLLIN;
                        ev.data.ptr = uth;
                        epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, uth->fd, &ev);
                    }
                } else if (n == 0) {
                    //TODO: peer down. this can not happen
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        ev.events = EPOLLIN;
                        ev.data.ptr = uth;
                        epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, uth->fd, &ev);
                    } else {
                        close(uth->fd);
                        w->uth_alive--;
                    }
                }
            }

            if (w->epoll_events[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
                uth = w->epoll_events[i].data.ptr;

                ev.events = EPOLLOUT;
                epoll_ctl(w->epoll_fd, EPOLL_CTL_DEL, uth->fd, &ev);

                //jff_log(JFF_INFO, "write %d", uth->fd);
                n = write(uth->fd, "x", 1);

                if (n < 0) {
                    err = errno;

                    if (err == EAGAIN || err == EWOULDBLOCK) {
                        ev.events = EPOLLOUT;
                        ev.data.ptr = uth;
                        epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, uth->fd, &ev);
                    } else {
                        close(uth->fd);
                        w->uth_alive--;
                    }
                } else {
                    uth->recv_buf_pos = 0;
                    uth->recv_buf_size = 192;
                    //jff_log(JFF_INFO, "write 'x' to server success");

                    ev.events = EPOLLIN;
                    ev.data.ptr = uth;
                    epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, uth->fd, &ev);
                }
            }
        }
    }

    //jff_log(JFF_INFO, "net worker finish");

    close(w->epoll_fd);
    free(w->epoll_events);

    return 0;
}

static int process_response(int fd, char recv_buf[], int len)
{
    int           linum, length, offset, real_offset, n;
    char         *q, *p;

    if (recv_buf[188] == 'C' && recv_buf[189] == 'O' &&
        recv_buf[190] == 'B' && recv_buf[191] == 'B') {
        /* work finished */
        p = q = recv_buf;
        while (*q != ' ') q++;
        linum = jff_atoi(p, q - p);

        buffer[linum].linum = -2;

        //jff_log(JFF_INFO, "recv finish");

        /* -2 means request finished */
        return -2;
    }

    sscanf(recv_buf, "%d %d %d ", &linum, &length, &offset);

    buffer[linum].linum = linum;
    buffer[linum].length = length;
    memcpy(buffer[linum].data, recv_buf + 40, length);

    /*
    p = q = recv_buf;
    while (*q != ' ') q++;
    linum = jff_atoi(p, q - p);
    n = q - p;

    p = q = q + 1;
    while(*q != ' ') q++;
    length = jff_atoi(p, q - p);

    p = q = q + 1;
    while(*q != ' ') q++;
    offset = jff_atoi(p, q - p);


    real_offset = count_real_offset(offset, linum);

    lseek(fd, real_offset, SEEK_SET);
    write(fd, recv_buf + 40, length);
    */
    return OK;
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

static inline int count_real_offset(int offset, int linum)
{
    int m, n, bits, pw;
    char      buf[16];

    /*
    bits = 0;
    m = linum;
    while (m) {
      bits++;
      m = m % 10;
    }
    */
    bits = sprintf(buf, "%d", linum);

    pw = 1;
    for (m = 0; m < bits - 1; m ++) {
        pw *= 10;
    }
    n = linum - pw;

    return offset + n * bits + off_map[bits - 1];
}
