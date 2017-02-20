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

#define OUTPUT_PLUGIN_NAME "websocket output plugin"

static pthread_t worker;
static globals *pglobal;

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
        pglobal->in.add(conn);
    }
    close(server_sockfd);
    pthread_cleanup_pop(1);
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

        /* p, port */
        case 2:
        case 3:
            DBG("case 2,3\n");
            port = atoi(optarg);
            break;

        /* q, queue */
        case 4:
        case 5:
            queue = atoi(optarg);
            DBG("case 4,5\n");
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
int output_run() {
    DBG("launching worker thread\n");
    pthread_create(&worker, 0, worker_thread, NULL);
    pthread_detach(worker);
    return 0;
}

int output_add() {
    return 0;
}
