
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

#include "../../audio_pipe.h"
#include "../../utils.h"

#define INPUT_PLUGIN_NAME "ALSA input plugin"
#define MAX_ARGUMENTS 32

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;

void *worker_thread( void *);
void worker_cleanup(void *);
void help(void);

static int plugin_number;

int init_alsa();
//alsa init parameter
char *dev = "default";
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
unsigned int rate = 16000;
unsigned int channels = 1;
snd_pcm_t *capture_handle;
int buffer_frames = 512;
int buffer_length = 1024;

/*** plugin interface functions ***/
int input_init(input_parameter *param) {
    int i;
    plugin_number = id;

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


    param->global->in[id].name = malloc((strlen(INPUT_PLUGIN_NAME) + 1) * sizeof(char));
    sprintf(param->global->in[id].name, INPUT_PLUGIN_NAME);
    /* allocate memory for frame */
    param->global->in[id].buf = malloc(buffer_frames * snd_pcm_format_width(format)/8 * channels);
    if(param->global->in[plugin_number].buf == NULL) {
        fprintf(stderr, "could not allocate memory\n");
        return 1;
    }

    return 0;
}

int input_stop() {
    DBG("will cancel input thread\n");
    pthread_cancel(worker);
    return 0;
}

int input_run()
{
    /*init alsa*/
    if (init_alsa()) {
        IPRINT("init_alsa failed\n");
        closelog();
        return 1;
    }

    if( pthread_create(&worker, 0, worker_thread, NULL) != 0) {
        free(pglobal->in[id].buf);
        fprintf(stderr, "could not start worker thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_detach(worker);

    return 0;
}

int input_add(int id)
{
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
    int err;
    int buffer_frames = 512;
    int buffer_length = buffer_frames * snd_pcm_format_width(format)/8 * channels;
    char buffer[buffer_length];
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);
    while( !pglobal->stop ) {
        pthread_mutex_lock(&pglobal->in[plugin_number].db);
        if ((err = snd_pcm_readi (capture_handle, buffer, buffer_frames)) != buffer_frames) {
            fprintf (stderr, "read from audio interface failed %d:(%s)\n",
                     err, snd_strerror (err));
            free(pglobal->in[plugin_number].buf);
            pglobal->in[plugin_number].size = 0;
            break;
        }
        /* copy frame from alsa to global buffer */
        DBG("read frame from alsa\n");

//        DBG("input locked\n");
        memcpy(pglobal->in[plugin_number].buf, buffer, buffer_length);
        pglobal->in[plugin_number].size = buffer_length;
//        DBG(pglobal->in[plugin_number].buf);
        /* signal fresh_frame */
        pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
//        DBG("signaled all wait thread\n");
        pthread_mutex_unlock(&pglobal->in[plugin_number].db );
//        DBG("input unlocked\n");
    }
    DBG("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);

    return NULL;
}

void worker_cleanup(void *arg) {
    static unsigned char first_run=1;

    if ( !first_run ) {
        DBG("already cleaned up ressources\n");
        return;
    }


    first_run = 0;
    DBG("cleaning up ressources allocated by input thread\n");
    if(pglobal->in[plugin_number].buf != NULL)
        free(pglobal->in[plugin_number].buf);

    snd_pcm_close (capture_handle);
    DBG("audio interface closed\n");
}

int init_alsa()
{
    int err;
    snd_pcm_hw_params_t *hw_params;

    if ((err = snd_pcm_open (&capture_handle, dev, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%s)\n",
                 dev,
                snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "audio interface opened\n");

    if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "hw_params allocated\n");

    if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
        fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "hw_params initialized\n");

    if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf (stderr, "cannot set access type (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "hw_params access setted\n");

    if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, format)) < 0) {
        fprintf (stderr, "cannot set sample format (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "hw_params format setted\n");

    if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
        fprintf (stderr, "cannot set sample rate (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "hw_params rate setted\n");

    if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, channels)) < 0) {
        fprintf (stderr, "cannot set channel count (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "hw_params channels setted\n");

    if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
        fprintf (stderr, "cannot set parameters (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "hw_params setted\n");

    snd_pcm_hw_params_free (hw_params);

    fprintf(stdout, "hw_params freed\n");

    if ((err = snd_pcm_prepare (capture_handle)) < 0) {
        fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    fprintf(stdout, "audio interface prepared\n");

    return 0;
}


