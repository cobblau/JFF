#ifndef _JFF_SOCK_H
#define _JFF_SOCK_H

#include "jff_config.h"

int tcp_listen(int listen_port);
int tcp_connect(char *serv_ip, int serv_port);

int tcp_set_keepalive(int fd);
int tcp_set_nodelay(int fd);
int tcp_set_nonblock(int fd);
int set_rlimit();

int listen_fd;

#endif
