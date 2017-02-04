
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "../../wave_streamer.h"
#include "../../utils.h"
#include "../../tools/alsa.h"

#define INPUT_PLUGIN_NAME "ALSA input plugin"

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;

void *worker_thread( void *);
void worker_cleanup(void *);
void help(void);

//alsa init parameter
char *dev = "default";
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
unsigned int rate = 16000;
unsigned int channels = 1;
int buffer_frames = 512;
int buffer_length = 1024;

typedef struct _client_para
{
    snd_pcm_t *handle;
    int pipe_fd[2];
}client_para;

/*** plugin interface functions ***/
int input_init(input_parameter *param) {
    int i;
    param->argv[0] = INPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c=0, format_temp = 16;
        static struct option long_options[] =
        {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"d", required_argument, 0, 0},
            {"device", required_argument, 0, 0},
            {"f", required_argument, 0, 0},
            {"format", required_argument, 0, 0},
            {"r", required_argument, 0, 0},
            {"rate", required_argument, 0, 0},
            {"c", required_argument, 0, 0},
            {"channels", required_argument, 0, 0},
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

            /* d, device */
        case 2:
        case 3:
            DBG("case 2,3\n");
            dev = strdup(optarg);
            break;

            /* f, format */
        case 4:
        case 5:
            DBG("case 4,5\n");
            format_temp = atoi(optarg);
            if (format_temp == 8)
                format = SND_PCM_FORMAT_S8;
            break;

            /* r, rate*/
        case 6:
        case 7:
            DBG("case 6,7\n");
            rate = atoi(optarg);
            break;

            /* c, channels*/
        case 8:
        case 9:
            DBG("case 8,9\n");
            channels = atoi(optarg);
            break;
        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    pglobal = param->global;

//    IPRINT("JPG input folder..: %s\n", folder);
//    IPRINT("delay.............: %i\n", delay);


    param->global->in.name = malloc((strlen(INPUT_PLUGIN_NAME) + 1) * sizeof(char));
    sprintf(param->global->in.name, INPUT_PLUGIN_NAME);

    return 0;
}

int input_stop() {
    DBG("will cancel input thread\n");
    return 0;
}

int input_run()
{
    return 0;
}

int input_add(int pipe_fd)
{
//    client_para cli_para;
//    cli_para.pipe_fd[0] = pipe_fd[0];
//    cli_para.pipe_fd[1] = pipe_fd[1];

    if( pthread_create(&worker, 0, worker_thread, (void *)pipe_fd) != 0) {
        fprintf(stderr, "could not start worker thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_detach(worker);
    return 0;
}

/*** private functions for this plugin below ***/
void help(void) {
    fprintf(stderr, " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
    " [-d | --delay ]........: delay to pause between frames\n" \
    " [-f | --folder ].......: folder to watch for new JPEG files\n" \
    " [-r | --remove ].......: remove/delete JPEG file after reading\n" \
    " [-n | --name ].........: ignore changes unless filename matches\n" \
    " [-e | --existing ].....: serve the existing *.jpg files from the specified directory\n" \
    " ---------------------------------------------------------------\n");
}

/* the single writer thread */
void *worker_thread( void *arg ) {
//    client_para* cli_para = (client_para*)arg;
    int output_fd = (int) arg;
    snd_pcm_t *capture_handle;
    init_alsa(&capture_handle,dev,rate,format,channels);
    int err;
    int buffer_frames = 512;
    int buffer_length = buffer_frames * snd_pcm_format_width(format)/8 * channels;
    char buffer[buffer_length];
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);
//    close(cli_para->pipe_fd[0]);
    while( !pglobal->stop ) {
        if ((err = snd_pcm_readi (capture_handle, buffer, buffer_frames)) != buffer_frames) {
            fprintf (stderr, "read from audio interface failed %d:(%s)\n",
                     err, snd_strerror (err));
            break;
        }
        /* copy frame from alsa to global buffer */
        //DBG("read frame from alsa\n");
        if ((err = write(output_fd,buffer,buffer_length))<=0) {
            perror( "write output_fd filed");
            break;
        }
    }
    DBG("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);
//    close(cli_para->pipe_fd[1]);
    snd_pcm_close(capture_handle);
    return NULL;
}

void worker_cleanup(void *arg) {
    static unsigned char first_run=1;

    if ( !first_run ) {
        DBG("already cleaned up ressources\n");
        return;
    }


    first_run = 0;
    DBG("audio interface closed\n");
}



