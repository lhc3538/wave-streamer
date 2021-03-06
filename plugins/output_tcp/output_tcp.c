#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#include "../../utils.h"
#include "../../wave_streamer.h"
#include "../../tools/tcp.h"

#define OUTPUT_PLUGIN_NAME "tcp output plugin"

static pthread_t worker;
static globals *pglobal;

char *type = "server";
char *ip = "127.0.0.1";
int port = 8081,queue = 20;
/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this plugin:\n\n" \
            " [-f | --folder ]........: folder to save pictures\n" \
            " [-m | --mjpeg ].........: save the frames to an mjpg file \n" \
            " [-d | --delay ].........: delay after saving pictures in ms\n" \
            " [-i | --input ].........: read frames from the specified input plugin\n" \
            " The following arguments are takes effect only if the current mode is not MJPG\n" \
            " [-s | --size ]..........: size of ring buffer (max number of pictures to hold)\n" \
            " [-e | --exceed ]........: allow ringbuffer to exceed limit by this amount\n" \
            " [-c | --command ].......: execute command after saving picture\n"\
            " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: clean up allocated ressources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg) {
    static unsigned char first_run=1;

    if ( !first_run ) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;

    OPRINT("cleaning up ressources allocated by worker thread\n");

}

void *worker_in_thread( void *arg )
{
    int sock_fd = (int) arg;
    int pipe_fd[2];
    char buffer[1024];

    if (pipe(pipe_fd) == -1)
    {
        perror("pipe init failed");
        return;
    }

    /* start input plug thread */
    pglobal->in.add_in(pipe_fd);

    while(!pglobal->stop)
    {
        if (read(sock_fd,buffer,sizeof(buffer))<=0)
        {
            perror("read socket failed");
            break;
        }
        if (write(pipe_fd[1],buffer,sizeof(buffer))<=0)
        {
            perror("write pipe failed");
            break;
        }
    }
    close(sock_fd);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
}

void *worker_out_thread( void *arg )
{
    int sock_fd = (int) arg;
    int pipe_fd[2];
    char buffer[1024];

    if (pipe(pipe_fd) == -1)
    {
        perror("pipe init failed");
        return;
    }

    /* start input plug thread */
    pglobal->in.add_out(pipe_fd);

    while(!pglobal->stop)
    {
        if (read(pipe_fd[0],buffer,sizeof(buffer))<=0)
        {
            perror("read pipe");
            break;
        }
        if (write(sock_fd,buffer,sizeof(buffer))<=0)
        {
            perror("write socket failed");
            break;
        }
//        printf("data:%s",buffer);
    }
    close(sock_fd);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
}

/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame and stores it to file
Input Value.:
Return Value:
******************************************************************************/
void *worker_thread( void *arg )
{
    /*create socket fd*/
    int server_sockfd = passive_server(port,queue);

    struct sockaddr_in client_addr;
    socklen_t length = sizeof(client_addr);
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);
    while(!pglobal->stop)
    {
        DBG("等待客户端连接\n");

        ///成功返回非负描述字，出错返回-1
        int conn = accept(server_sockfd, (struct sockaddr*)&client_addr, &length);
        if(conn<0)
        {
            perror("connect");
            exit(1);
        }
        DBG("客户端成功连接,socketID=%d\n",conn);
        /* output stream thread */
        pthread_t  worker_out;
        pthread_create(&worker_out, 0, worker_out_thread, (void*)conn);
        pthread_detach(worker_out);
        /* input stream thread */
        pthread_t  worker_in;
        pthread_create(&worker_in, 0, worker_in_thread, (void*)conn);
        pthread_detach(worker_in);
    }
    close(server_sockfd);
    pthread_cleanup_pop(1);
    return NULL;
}

/**
 * @brief client_thread
 * tcp socket client type thread
 * @param arg
 * @return
 */
void *client_thread( void *arg )
{
    /*create socket fd*/
    int client_sockfd = connect_server(ip,port);
    if (client_sockfd < 0)
    {
        perror("connect server failed");
        exit(1);
    }

    /* output stream thread */
    pthread_t  worker_out;
    pthread_create(&worker_out, 0, worker_out_thread, (void*)client_sockfd);
    pthread_detach(worker_out);
    /* input stream thread */
    pthread_t  worker_in;
    pthread_create(&worker_in, 0, worker_in_thread, (void*)client_sockfd);
    pthread_detach(worker_in);

    return NULL;
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: this function is called first, in order to initialise
              this plugin and pass a parameter string
Input Value.: parameters
Return Value: 0 if everything is ok, non-zero otherwise
******************************************************************************/
int output_init(output_parameter *param)
{
    int i;
    pglobal = param->global;
    pglobal->out.name = malloc((1+strlen(OUTPUT_PLUGIN_NAME))*sizeof(char));
    sprintf(pglobal->out.name, "%s", OUTPUT_PLUGIN_NAME);

    DBG("OUT plugin name: %s\n", pglobal->out.name);

    param->argv[0] = OUTPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c=0;
        static struct option long_options[] = \
        {
        {"h", no_argument, 0, 0},
        {"help", no_argument, 0, 0},
        {"t", required_argument, 0, 0},
        {"type", required_argument, 0, 0},
        {"i", required_argument, 0, 0},
        {"ip", required_argument, 0, 0},
        {"p", required_argument, 0, 0},
        {"port", required_argument, 0, 0},
        {"q", required_argument, 0, 0},
        {"queue", required_argument, 0, 0},
        {0, 0, 0, 0}
    };

        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if (c == -1) break;

        /* unrecognized option */
        if (c == '?'){
            help();
            return 1;
        }

        switch (option_index) {
        /* h, help */
        case 0:
        case 1:
            DBG("case 0,1\n");
            help();
            return 1;
            break;

        /* t, type */
        case 2:
        case 3:
            DBG("case 2,3\n");
            type = strdup(optarg);
            break;

        /* i, ip */
        case 4:
        case 5:
            DBG("case 4,5\n");
            ip = strdup(optarg);
            break;

        /* p, port */
        case 6:
        case 7:
            DBG("case 6,7\n");
            port = atoi(optarg);
            break;

        /* q, queue */
        case 8:
        case 9:
            queue = atoi(optarg);
            DBG("case 8,9\n");
            break;

        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    OPRINT("input plugin.....: %s\n",  pglobal->in.plugin);

    return 0;
}

/******************************************************************************
Description.: calling this function stops the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_stop() {
    DBG("will cancel worker thread\n");
    pthread_cancel(worker);
    return 0;
}

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_run()
{
    DBG("launching worker thread %s\n",type);
    if (strcmp(type,"server") == 0)
        pthread_create(&worker, 0, worker_thread, NULL);
    else if (strcmp(type,"client") == 0)
        pthread_create(&worker, 0, client_thread, NULL);
    else
    {
        OPRINT("Bad parameter in type:%s\n",  type);
        return -1;
    }
    pthread_detach(worker);
    return 0;
}

int output_add() {
    return 0;
}
