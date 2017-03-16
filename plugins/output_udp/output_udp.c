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

#define OUTPUT_PLUGIN_NAME "udp output plugin"

typedef struct
{
    unsigned long long id; //package's id
    unsigned char data[BUFFER_LENGTH];    //audio data
} Package;

static globals *pglobal;

int sock_fd;
int port = 8081;
struct sockaddr_in addr_cache;
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
    Package pack;
    char buf[sizeof(Package)];
    int n,addr_len = sizeof(addr_cache);
    unsigned long long current_id = 0;

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe init failed");
        return;
    }

    /* start input plug thread */
    pglobal->in.add_in(pipe_fd);

    while(!pglobal->stop)
    {
        n = recvfrom(sock_fd, buf, sizeof(Package), 0, (struct sockaddr *)&addr_cache, &addr_len);
        if (n <= 0)
        {
            perror("recvfrom err");
//            break;
        }
        else
        {
            memcpy(&pack,buf,sizeof(Package));
            if (pack.id > current_id)
            {
                if (write(pipe_fd[1],buffer,sizeof(buffer))<=0)
                {
                    perror("write pipe failed");
                    break;
                }
                current_id = pack.id;
            }

        }
    }
    close(sock_fd);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
}

void *worker_out_thread( void *arg )
{
    int sock_fd = (int) arg;
    Package pack;
    char buf[sizeof(Package)];
    int n,addr_len = sizeof(addr_cache);
    unsigned long long current_id = 0;

    int pipe_fd[2];
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
/**
 * @brief initSock
 * init udp socket server
 */
int init_sock()
{
    struct sockaddr_in addr;
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("create sock_phone failed!");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind error");
        return -1;
    }
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
        {"p", required_argument, 0, 0},
        {"port", required_argument, 0, 0},
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

        /* p, port */
        case 2:
        case 3:
            DBG("case 2,3\n");
            port = atoi(optarg);
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

    /*create socket fd*/
    if(init_sock() < 0)
        return NULL;

    DBG("客户端成功连接,socketID=%d\n",conn);
    /* output stream thread */
    pthread_t  worker_out;
    pthread_create(&worker_out, 0, worker_out_thread, NULL);
    pthread_detach(worker_out);
    /* input stream thread */
    pthread_t  worker_in;
    pthread_create(&worker_in, 0, worker_in_thread, NULL);
    pthread_detach(worker_in);

    return 0;
}

int output_add() {
    return 0;
}
