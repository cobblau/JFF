#ifndef _JFF_CONFIG_H
#define _JFF_CONFIG_H

#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include "jff_log.h"

#define  OK     0
#define  ERROR -1

#define CR  0x0d
#define LF  0x0a

#define MAX_LINE 30000000

struct client_s {
    char *server_addr;
    int   server_port;
    int   ncpus;
    int   concurrent_num;
    char *data_file_name;

    int fd;
};

typedef struct client_s client_t;

typedef struct element_s {
    int  linum;
    int  length;
    char data[256];
} element_t;

#endif
