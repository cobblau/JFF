#include "jff_config.h"

int
tcp_connect(char *serv_ip, int serv_port)
{
    int sockfd;
    struct sockaddr_in servaddr;

    /* init servaddr */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(serv_port);

    if(inet_pton(AF_INET, serv_ip, &servaddr.sin_addr) <= 0) {
        jff_log(JFF_ERROR, "[%s] is not a valid IPaddress\n", serv_ip);
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        jff_log(JFF_ERROR, "connect_error");
        return -1;
    }

    return sockfd;
}


int
tcp_listen(int listen_port)
{
    int listen_fd;
    struct sockaddr_in serv_addr;
	int val, yes = 1;


    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "create socket error!");
        return -1;
    }

    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	/* Make socket non-block. */
	val = fcntl(listen_fd, F_GETFL, 0);
	fcntl(listen_fd, F_SETFL, val | O_NONBLOCK);

	/* Set remote IP and connect */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(listen_port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);;

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        fprintf(stderr, "bind error!\n");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 1024) == -1) {
        fprintf(stderr, "listen error\n");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

int tcp_set_nonblock(int fd)
{
    int val;

    val = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, val | O_NONBLOCK);

    return 0;
}
int tcp_set_keepalive(int fd)
{
    int tag = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &tag, sizeof(tag)) < 0) {
        jff_log(JFF_ERROR, "set sock keepalive error");
        return -1;
    }

    return 0;
}

int tcp_set_nodelay(int fd)
{
    int tag = 1;

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &tag, sizeof(tag)) < 0) {
        jff_log(JFF_ERROR, "set sock nodelay error");
        return -1;
    }

    return 0;
}


int set_rlimit()
{
    struct rlimit limit;

    limit.rlim_cur = limit.rlim_max = 8192;
    if (setrlimit(RLIMIT_NOFILE, &limit) == -1) {
        jff_log(JFF_ERROR, "can't set rlimit:%s", strerror(errno));
        return -1;
    }

    return 0;
}
