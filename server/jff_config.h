#ifndef _JFF_CONFIG_H
#define _JFF_CONFIG_H

#define _GNU_SOURCE
#define __USE_GNU

#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdarg.h>
#include <stddef.h>

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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>

#include "jff_log.h"

#define  OK     0
#define  ERROR -1

#define CR  0x0d
#define LF  0x0a

struct server_s {
    int   listen_fd;
    int   listen_port;
    int   max_connection;
    int   ncpus;
    char *data_file_name;
};

typedef struct server_s server_t;

struct client_s {
    char *server_addr;
    int   server_port;
    int   concurrent_num;
    char *data_file_name;
};

typedef struct client_s client_t;

typedef struct element_s {
    int  linum;
    int  length;
    int  offset;
    char data[256];
} element_t;

#endif
