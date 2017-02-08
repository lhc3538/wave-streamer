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
#include <syslog.h>

#include "../../wave_streamer.h"
#include "../../utils.h"
#include "httpd.h"

#define OUTPUT_PLUGIN_NAME "HTTP output plugin"

static pthread_t worker;
static globals *pglobal;
/******************************************************************************
Description.: print help for this plugin to stdout
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this plugin:\n\n" \
            " [-w | --www ]...........: folder that contains webpages in \n" \
            "                           flat hierarchy (no subfolders)\n" \
            " [-p | --port ]..........: TCP port for this HTTP server\n" \
            " [-c | --credentials ]...: ask for \"username:password\" on connect\n" \
            " [-n | --nocommands ]....: disable execution of commands\n"
            " ---------------------------------------------------------------\n");
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: Initialize this plugin.
              parse configuration parameters,
              store the parsed values in global variables
Input Value.: All parameters to work with.
              Among many other variables the "param->id" is quite important -
              it is used to distinguish between several server instances
Return Value: 0 if everything is OK, other values signal an error
******************************************************************************/
int output_init(output_parameter *param)
{
    int i;
    int  port;
    char *credentials, *www_folder;
    char nocommands;

    DBG("output #\n");

    port = htons(8080);
    credentials = NULL;
    www_folder = NULL;
    nocommands = 0;

    param->argv[0] = OUTPUT_PLUGIN_NAME;
    param->global->out.name = malloc((strlen(OUTPUT_PLUGIN_NAME) + 1) * sizeof(char));
    sprintf(param->global->out.name, OUTPUT_PLUGIN_NAME);
    pglobal = param->global;
    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0
            },
            {"help", no_argument, 0, 0},
            {"p", required_argument, 0, 0},
            {"port", required_argument, 0, 0},
            {"c", required_argument, 0, 0},
            {"credentials", required_argument, 0, 0},
            {"w", required_argument, 0, 0},
            {"www", required_argument, 0, 0},
            {"n", no_argument, 0, 0},
            {"nocommands", no_argument, 0, 0},
            {0, 0, 0, 0}
        };

        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        switch(option_index) {
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
            port = htons(atoi(optarg));
            break;

            /* c, credentials */
        case 4:
        case 5:
            DBG("case 4,5\n");
            credentials = strdup(optarg);
            break;

            /* w, www */
        case 6:
        case 7:
            DBG("case 6,7\n");
            www_folder = malloc(strlen(optarg) + 2);
            strcpy(www_folder, optarg);
            if(optarg[strlen(optarg)-1] != '/')
                strcat(www_folder, "/");
            break;

            /* n, nocommands */
        case 8:
        case 9:
            DBG("case 8,9\n");
            nocommands = 1;
            break;
        }
    }


    OPRINT("www-folder-path...: %s\n", (www_folder == NULL) ? "disabled" : www_folder);
    OPRINT("HTTP TCP port.....: %d\n", ntohs(port));
    OPRINT("username:password.: %s\n", (credentials == NULL) ? "disabled" : credentials);
    OPRINT("commands..........: %s\n", (nocommands) ? "disabled" : "enabled");

    return 0;
}

/******************************************************************************
Description.: this will stop the server thread, client threads
              will not get cleaned properly, because they run detached and
              no pointer is kept. This is not a huge issue, because this
              funtion is intended to clean up the biggest mess on shutdown.
Input Value.: id determines which server instance to send commands to
Return Value: always 0
******************************************************************************/
int output_stop()
{

    DBG("will cancel server thread #\n");
    pthread_cancel(worker);
    return 0;
}

/******************************************************************************
Description.: This creates and starts the server thread
Input Value.: id determines which server instance to send commands to
Return Value: always 0
******************************************************************************/
int output_run()
{
    DBG("launching server thread #\n");

    /* create thread and pass context to thread function,this thread order to deal client's connection */
    pthread_create(&worker, NULL, server_thread, (void*)pglobal);
    pthread_detach(worker);

    return 0;
}

int output_add(int fd)
{
    return 0;
}
