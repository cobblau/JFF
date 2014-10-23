#include "jff_config.h"
#include "jff_sock.h"
#include "jff_worker.h"
#include "jff_ring.h"

#define  MAX_LIN     10474514
#define  MAX_LINE    48000000
#define  PORT        1248
#define  MAX_CONNS   102400



jff_ring_t ring;
struct server_s  server;

int print_usage()
{
    fprintf(stderr, "\n"
            "Usage: server [-d datafile] [-c conns]\n"
            "  -p specifies the listen port. default %d\n"
            "  -c max_connections for a thread. default %d\n"
            "  -d specifies the datafile\n\n", PORT, MAX_CONNS);

    return 0;
}

int parse_args(int argc, char *argv[])
{
    int   c;
    char *data_file;

    while((c = getopt(argc, argv, "p:d:h")) != -1) {
        switch(c) {
        case 'c':
            server.max_connection = atoi(optarg);
            break;

        case 'd':
            if ((data_file = malloc(strlen(optarg) + 1)) == NULL) {
                fprintf(stderr, "malloc error!\n");
                return -1;
            }

            memset(data_file, '\0', strlen(optarg) + 1);
            memcpy(data_file, optarg, strlen(optarg));

            server.data_file_name = data_file;

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

int server_setup(int argc, char *argv[])
{

    server.listen_port = PORT;
    server.max_connection = MAX_CONNS;

    if (parse_args(argc, argv) != 0) {
        return ERROR;
    }

    if (jff_ring_init(&ring, MAX_LINE) == ERROR) {
        return ERROR;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int  listen_fd;


    if(server_setup(argc, argv) == ERROR) {
        jff_log(JFF_ERROR, "server setup error");
        return -1;
    }

    if ((listen_fd = tcp_listen(PORT)) == -1) {
        return -1;
    }

    server.listen_fd = listen_fd;

    server.ncpus = sysconf(_SC_NPROCESSORS_CONF);

    if (workers_init(&server) != 0) {
        jff_log(JFF_ERROR, "init workers error!\n");
        goto finish;
    }

    /*
    if (workers_wakeup() != 0) {
        jff_log(JFF_ERROR, "create wokers error!\n");
        goto finish;
    }
    */

    if (workers_fun() != 0) {
        jff_log(JFF_ERROR, "whimp wokers error!\n");
        goto finish;
    }

    if (workers_off_duty() != 0) {
        jff_log(JFF_ERROR, "wokers off duty error!\n");
        goto finish;
    }

    if (workers_destroy() != 0) {
        jff_log(JFF_ERROR, "wokers off duty error!\n");
        goto finish;
    }

 finish:
    close(listen_fd);

    return 0;
}
