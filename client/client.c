#include "jff_config.h"
#include "jff_sock.h"
#include "jff_worker.h"


struct client_s  client;
element_t buffer[MAX_LINE];
int off_map[16];

int print_usage()
{
    fprintf(stderr, "\n"
            "Usage: server [-s ip] [-p port] [-d datafile] [-t conccurent_num]\n"
            "  -s server's ip address. \n"
            "  -p server's port. \n"
            "  -t connections per thread \n"
            "  -d specifies the datafile \n");

    return 0;
}

int parse_args(int argc, char *argv[])
{
    int   c;
    char *data_file, *addr;

    while((c = getopt(argc, argv, "s:p:d:t:h")) != -1) {
        switch(c) {
        case 's':
            if ((addr = malloc(strlen(optarg) + 1)) == NULL) {
                jff_log(JFF_ERROR, "malloc error!\n");
                return -1;
            }

            memset(addr, '\0', strlen(optarg) + 1);
            memcpy(addr, optarg, strlen(optarg));

            client.server_addr = addr;

            break;

        case 'p':
            client.server_port = atoi(optarg);
            break;

        case 't':
            client.concurrent_num = atoi(optarg);
            break;

        case 'd':
            if ((data_file = malloc(strlen(optarg) + 1)) == NULL) {
                jff_log(JFF_ERROR, "malloc error!\n");
                return -1;
            }

            memset(data_file, '\0', strlen(optarg) + 1);
            memcpy(data_file, optarg, strlen(optarg));

            client.data_file_name = data_file;

            break;

        case 'h':
            print_usage();
            exit(-1);

        default:
            fprintf(stderr, "invalid option:%s\n", optarg);
            return -1;
        }
    }

    return 0;
}

int fill_off_map()
{
  int i;

  off_map[0] = 1;
  off_map[1] = 10;
  off_map[2] = 180 + off_map[1];
  off_map[3] = 2700 + off_map[2];
  off_map[4] = 36000 + off_map[3];
  off_map[5] = 450000 + off_map[4];
  off_map[6] = 5400000 + off_map[5];
  off_map[7] = 63000000 + off_map[6];
  off_map[8] = 720000000 + off_map[7];
  off_map[9] = 8100000000 + off_map[8];

  return 0;
}
int client_setup(int argc, char *argv[])
{
    int i, fd;

    client.ncpus = sysconf(_SC_NPROCESSORS_CONF);

    if (parse_args(argc, argv) != 0) {
        return ERROR;
    }

    if ((fd = open(client.data_file_name, O_RDWR | O_CREAT, 0666)) == -1) {
        jff_log(JFF_ERROR, "open file:%s error:%s", client.data_file_name, strerror(errno));
	return -1;
    }

    client.fd = fd;

    return 0;
}

int main(int argc, char *argv[])
{
    struct timeval stop, start, mid;

    gettimeofday(&start, NULL);

    if(client_setup(argc, argv) == ERROR) {
        jff_log(JFF_ERROR, "server setup error");
        return -1;
    }

    if (workers_init(&client) != 0) {
        jff_log(JFF_ERROR, "init client workers error!\n");
        goto finish;
    }

    if (workers_fun() != 0) {
        jff_log(JFF_ERROR, "whimp client wokers error!\n");
        goto finish;
    }

    if (workers_off_duty() != 0) {
        jff_log(JFF_ERROR, "client wokers off duty error!\n");
        goto finish;
    }

    if (workers_destroy() != 0) {
        jff_log(JFF_ERROR, "client wokers off duty error!\n");
        goto finish;
    }

    gettimeofday(&mid, NULL);
    printf("recv data cost=%lums\n", (mid.tv_sec - start.tv_sec) * 1000 + (mid.tv_usec - start.tv_usec) / 1000);

    int i;
    for (i = 0; i < MAX_LINE; i++) {
        if (buffer[i].linum == -2) {
            break;
        }

        write(client.fd, buffer[i].data, buffer[i].length);
    }

    close(client.fd);
    gettimeofday(&stop, NULL);
    printf("cost=%lums\n", (stop.tv_sec - start.tv_sec) * 1000 + (stop.tv_usec - start.tv_usec) / 1000);

 finish:

    return 0;
}
